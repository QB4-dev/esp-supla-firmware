/*
 * device.c
 *
 *  Created on: 27 sty 2024
 *      Author: kuba
 */

#include "device.h"

/**
 * @brief Define device event base
 *
 */
ESP_EVENT_DEFINE_BASE(DEV_EVENT);

static const char *TAG = "DEV";

static EventGroupHandle_t      dev_event_group;
static TaskHandle_t            tsk_hdl;
static esp_event_loop_handle_t event_loop_hdl;

static void device_task_entry(void *arg)
{
    while (1) {
        esp_event_loop_run(event_loop_hdl, pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

esp_err_t device_init(esp_event_handler_t ev_hdl, void *args)
{
    esp_event_loop_args_t loop_args = { .queue_size = 4, .task_name = NULL };
    esp_err_t             esp_err;

    dev_event_group = xEventGroupCreate();
    if (!dev_event_group)
        return ESP_ERR_NO_MEM;

    esp_err = esp_event_loop_create(&loop_args, &event_loop_hdl);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "event loop create error");
        goto loop_failed;
    }

    esp_err =
        esp_event_handler_register_with(event_loop_hdl, DEV_EVENT, ESP_EVENT_ANY_ID, ev_hdl, args);
    if (esp_err != ESP_OK) {
        ESP_LOGE(TAG, "event handler register failed");
        goto eh_failed;
    }

    BaseType_t err = xTaskCreate(device_task_entry, "dev", 4096, NULL, tskIDLE_PRIORITY, &tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "task create failed");
        goto task_failed;
    }

    ESP_LOGI(TAG, "init OK");
    return ESP_OK;

task_failed:
eh_failed:
    esp_event_loop_delete(event_loop_hdl);
loop_failed:
    vEventGroupDelete(dev_event_group);
    return ESP_FAIL;
}

EventBits_t device_get_event_bits(void)
{
    return xEventGroupGetBits(dev_event_group);
}

esp_err_t device_init_config(void)
{
    EventBits_t bits = xEventGroupGetBits(dev_event_group);

    if (bits & DEVICE_CONFIG_EVENT_BIT)
        return ESP_OK;

    ESP_LOGW(TAG, "config init");
    xEventGroupSetBits(dev_event_group, DEVICE_CONFIG_EVENT_BIT);
    esp_event_post_to(event_loop_hdl, DEV_EVENT, DEVICE_EVENT_CONFIG_INIT, NULL, 0,
                      100 / portTICK_PERIOD_MS);
    return ESP_OK;
}

esp_err_t device_exit_config(void)
{
    EventBits_t bits = xEventGroupGetBits(dev_event_group);

    if (!(bits & DEVICE_CONFIG_EVENT_BIT))
        return ESP_OK;

    ESP_LOGW(TAG, "config exit");
    xEventGroupClearBits(dev_event_group, DEVICE_CONFIG_EVENT_BIT);
    esp_event_post_to(event_loop_hdl, DEV_EVENT, DEVICE_EVENT_CONFIG_EXIT, NULL, 0,
                      100 / portTICK_PERIOD_MS);
    return ESP_OK;
}
