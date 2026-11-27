/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/link-thermometer.h"
#include <cJSON.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
#include <esp_crt_bundle.h>
#endif

#define DEFAULT_POLL_INTERVAL_MS 10000
#define HTTP_TIMEOUT_MS 10000
#define MAX_RESPONSE_SIZE 1024
#define POLL_TASK_STACK_SIZE 8192

static const char *TAG = "link-thermometer";

struct http_response {
    char   body[MAX_RESPONSE_SIZE + 1];
    size_t size;
    bool   too_large;
};

struct link_thermometer_data {
    supla_channel_t  *channel;
    char             *link;
    TickType_t        poll_interval;
    TaskHandle_t      poll_task;
    SemaphoreHandle_t sample_mutex;
    SemaphoreHandle_t task_stopped;
    bool              has_sample;
    double            temperature;
};

static esp_err_t response_event_handler(esp_http_client_event_t *event)
{
    if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0)
        return ESP_OK;

    struct http_response *response = event->user_data;
    if (!response)
        return ESP_ERR_INVALID_ARG;

    size_t data_size = (size_t)event->data_len;
    if (data_size > MAX_RESPONSE_SIZE - response->size) {
        response->too_large = true;
        return ESP_OK;
    }

    memcpy(response->body + response->size, event->data, data_size);
    response->size += data_size;
    response->body[response->size] = '\0';
    return ESP_OK;
}

static esp_err_t parse_temperature(const char *json, double *temperature)
{
    cJSON *root = cJSON_Parse(json);
    if (!root)
        return ESP_ERR_INVALID_RESPONSE;

    cJSON    *connected = cJSON_GetObjectItem(root, "connected");
    cJSON    *temp = cJSON_GetObjectItem(root, "temperature");
    esp_err_t err = ESP_OK;

    if (!connected || connected->type != cJSON_True) {
        err = ESP_ERR_INVALID_STATE;
    } else if (!temp || temp->type != cJSON_Number || !isfinite(temp->valuedouble)) {
        err = ESP_ERR_INVALID_RESPONSE;
    } else {
        *temperature = temp->valuedouble;
    }

    cJSON_Delete(root);
    return err;
}

static esp_err_t fetch_temperature(const char *link, double *temperature)
{
    struct http_response     response = { 0 };
    esp_http_client_config_t config = {
        .url = link,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .event_handler = response_event_handler,
        .user_data = &response,
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
        .crt_bundle_attach = esp_crt_bundle_attach,
#endif
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
        return ESP_ERR_NO_MEM;

    esp_err_t err = esp_http_client_set_header(client, "Accept", "application/json");
    if (err == ESP_OK)
        err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code < 200 || status_code >= 300) {
            ESP_LOGW(TAG, "Thermometer returned HTTP status %d", status_code);
            err = ESP_FAIL;
        } else if (response.too_large) {
            err = ESP_ERR_INVALID_SIZE;
        } else {
            err = parse_temperature(response.body, temperature);
        }
    }

    esp_err_t cleanup_err = esp_http_client_cleanup(client);
    if (err == ESP_OK && cleanup_err != ESP_OK)
        err = cleanup_err;
    return err;
}

static void link_thermometer_poll_task(void *arg)
{
    struct link_thermometer_data *data = arg;

    for (;;) {
        double    temperature;
        esp_err_t err = fetch_temperature(data->link, &temperature);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Thermometer request failed: %s", esp_err_to_name(err));
        } else if (xSemaphoreTake(data->sample_mutex, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to lock cached temperature");
        } else {
            data->temperature = temperature;
            data->has_sample = true;
            xSemaphoreGive(data->sample_mutex);

            int rc = supla_channel_set_double_value(data->channel, temperature);
            if (rc != SUPLA_RESULT_TRUE)
                ESP_LOGE(TAG, "Failed to update thermometer channel: %d", rc);
        }

        if (ulTaskNotifyTake(pdTRUE, data->poll_interval) != 0)
            break;
    }

    xSemaphoreGive(data->task_stopped);
    vTaskSuspend(NULL);
}

supla_channel_t *link_thermometer_create(const struct link_thermometer_config *config)
{
    const supla_channel_config_t channel_config = {
        .type = SUPLA_CHANNELTYPE_THERMOMETER,
        .supported_functions = SUPLA_CHANNELFNC_THERMOMETER,
        .default_function = SUPLA_CHANNELFNC_THERMOMETER,
        .default_caption = "LINK-THERM",
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
        .sync_values_onchange = 1,
    };

    if (!config || !config->link || !config->link[0])
        return NULL;

    supla_channel_t *channel = supla_channel_create(&channel_config);
    if (!channel)
        return NULL;

    struct link_thermometer_data *data = calloc(1, sizeof(*data));
    if (!data) {
        supla_channel_free(channel);
        return NULL;
    }

    data->link = strdup(config->link);
    data->sample_mutex = xSemaphoreCreateMutex();
    data->task_stopped = xSemaphoreCreateBinary();
    data->channel = channel;

    uint32_t interval_ms = config->poll_interval_ms ? config->poll_interval_ms :
                                                      DEFAULT_POLL_INTERVAL_MS;
    data->poll_interval = pdMS_TO_TICKS(interval_ms);
    if (data->poll_interval == 0)
        data->poll_interval = 1;

    if (!data->link || !data->sample_mutex || !data->task_stopped) {
        ESP_LOGE(TAG, "Failed to allocate link thermometer resources");
        goto fail;
    }

    supla_channel_set_data(channel, data);
    if (xTaskCreate(link_thermometer_poll_task, "link-therm", POLL_TASK_STACK_SIZE, data,
                    tskIDLE_PRIORITY + 1, &data->poll_task) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create link thermometer task");
        supla_channel_set_data(channel, NULL);
        goto fail;
    }
    return channel;

fail:
    if (data->task_stopped)
        vSemaphoreDelete(data->task_stopped);
    if (data->sample_mutex)
        vSemaphoreDelete(data->sample_mutex);
    free(data->link);
    free(data);
    supla_channel_free(channel);
    return NULL;
}

esp_err_t link_thermometer_get_local(supla_channel_t *ch, double *temperature)
{
    if (!ch || !temperature)
        return ESP_ERR_INVALID_ARG;

    struct link_thermometer_data *data = supla_channel_get_data(ch);
    if (!data)
        return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(data->sample_mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    esp_err_t err = ESP_OK;
    if (!data->has_sample)
        err = ESP_ERR_INVALID_STATE;
    else
        *temperature = data->temperature;

    if (xSemaphoreGive(data->sample_mutex) != pdTRUE && err == ESP_OK)
        err = ESP_FAIL;
    return err;
}

int link_thermometer_delete(supla_channel_t *ch)
{
    if (!ch)
        return ESP_ERR_INVALID_ARG;

    struct link_thermometer_data *data = supla_channel_get_data(ch);
    if (!data)
        return ESP_ERR_INVALID_ARG;

    xTaskNotifyGive(data->poll_task);
    if (xSemaphoreTake(data->task_stopped, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    vTaskDelete(data->poll_task);
    supla_channel_set_data(ch, NULL);
    vSemaphoreDelete(data->task_stopped);
    vSemaphoreDelete(data->sample_mutex);
    free(data->link);
    free(data);
    return supla_channel_free(ch);
}
