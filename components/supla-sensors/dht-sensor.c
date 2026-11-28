/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/dht-sensor.h"
#include <stdlib.h>
#include <stdbool.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define DEFAULT_POLL_INTERVAL_MS 10000

static const char *TAG = "dht-sensor";

struct dht_channel_data {
    dht_sensor_type_t  sensor_type;
    gpio_num_t         gpio;
    esp_timer_handle_t timer;
    SemaphoreHandle_t  mutex;
    bool               has_sample;
    float              humidity;
    float              temperature;
};

esp_err_t supla_channel_dht_get_local(supla_channel_t *ch, float *humid, float *temp)
{
    if (!ch || !humid || !temp)
        return ESP_ERR_INVALID_ARG;

    struct dht_channel_data *dht_data = supla_channel_get_data(ch);
    if (!dht_data)
        return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(dht_data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    esp_err_t err = ESP_OK;
    if (!dht_data->has_sample) {
        err = ESP_ERR_INVALID_STATE;
    } else {
        *humid = dht_data->humidity;
        *temp = dht_data->temperature;
    }

    if (xSemaphoreGive(dht_data->mutex) != pdTRUE && err == ESP_OK)
        err = ESP_FAIL;
    return err;
}

static void dht_poll(void *arg)
{
    supla_channel_t         *ch = arg;
    struct dht_channel_data *dht_data = supla_channel_get_data(ch);
    float                    humidity, temperature;
    esp_err_t                rc;

    if (!dht_data) {
        ESP_LOGE(TAG, "DHT channel data is unavailable");
        return;
    }

    rc = dht_read_float_data(dht_data->sensor_type, dht_data->gpio, &humidity, &temperature);
    if (rc != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read DHT sensor: %s", esp_err_to_name(rc));
        dht_data->has_sample = false;
        return;
    }

    if (xSemaphoreTake(dht_data->mutex, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to lock DHT sample");
        return;
    }
    dht_data->humidity = humidity;
    dht_data->temperature = temperature;
    dht_data->has_sample = true;
    if (xSemaphoreGive(dht_data->mutex) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to unlock DHT sample");
        return;
    }

    rc = supla_channel_set_humidtemp_value(ch, humidity, temperature);
    if (rc != SUPLA_RESULT_TRUE)
        ESP_LOGE(TAG, "Failed to update DHT channel value: %d", rc);
}

supla_channel_t *supla_channel_dht_create(const struct dht_channel_config *config)
{
    const supla_channel_config_t temphumid_channel_config = {
        .type = SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR,
        .supported_functions = SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE,
        .default_function = SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE,
        .default_caption = "DHT",
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE
    };
    uint32_t                 poll_interval_ms;
    struct dht_channel_data *dht_data;
    esp_err_t                err;
    esp_timer_create_args_t  timer_args = {
         .name = "dht",
         .dispatch_method = ESP_TIMER_TASK,
         .callback = dht_poll,
    };

    if (!config)
        return NULL;

    supla_channel_t *ch = supla_channel_create(&temphumid_channel_config);
    if (!ch)
        return NULL;

    dht_data = calloc(1, sizeof(*dht_data));
    if (!dht_data) {
        supla_channel_free(ch);
        return NULL;
    }
    dht_data->mutex = xSemaphoreCreateMutex();
    if (!dht_data->mutex) {
        ESP_LOGE(TAG, "Failed to create DHT sample mutex");
        free(dht_data);
        supla_channel_free(ch);
        return NULL;
    }

    dht_data->sensor_type = config->sensor_type;
    dht_data->gpio = config->gpio;
    poll_interval_ms = config->poll_interval_ms ? config->poll_interval_ms :
                                                  DEFAULT_POLL_INTERVAL_MS;

    supla_channel_set_data(ch, dht_data);
    timer_args.arg = ch;

    err = esp_timer_create(&timer_args, &dht_data->timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create DHT timer: %s", esp_err_to_name(err));
        goto fail;
    }

    err = esp_timer_start_periodic(dht_data->timer, (uint64_t)poll_interval_ms * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start DHT timer: %s", esp_err_to_name(err));
        esp_err_t delete_err = esp_timer_delete(dht_data->timer);
        if (delete_err != ESP_OK)
            ESP_LOGE(TAG, "Failed to delete DHT timer after start failure: %s",
                     esp_err_to_name(delete_err));
        goto fail;
    }
    return ch;

fail:
    supla_channel_set_data(ch, NULL);
    vSemaphoreDelete(dht_data->mutex);
    free(dht_data);
    supla_channel_free(ch);
    return NULL;
}

int supla_channel_dht_delete(supla_channel_t *ch)
{
    if (!ch)
        return ESP_ERR_INVALID_ARG;

    struct dht_channel_data *dht_data = supla_channel_get_data(ch);
    if (!dht_data)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = esp_timer_stop(dht_data->timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    err = esp_timer_delete(dht_data->timer);
    if (err != ESP_OK)
        return err;

    supla_channel_set_data(ch, NULL);
    vSemaphoreDelete(dht_data->mutex);
    free(dht_data);
    return supla_channel_free(ch);
}
