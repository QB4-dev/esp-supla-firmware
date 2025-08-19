/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "include/esp-tuya-mcu.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

/**
 * @brief NMEA Parser runtime buffer size
 *
 */
#define TX_BUFFER_SIZE 256
#define RX_BUFFER_SIZE 256
#define TUYA_MCU_EVENT_LOOP_QUEUE_SIZE (16)

#define TUYA_MCU_TASK_STACK_SIZE (4096)
#define TUYA_MCU_TASK_PRIORITY (tskIDLE_PRIORITY)

ESP_EVENT_DEFINE_BASE(TUYA_MCU_EVENT);

static const char *TAG = "tuya_mcu";

/**
 * @brief TUYA MCU runtime structure
 *
 */
typedef struct {
    uart_port_t             uart_port;         /*!< Uart port number */
    tuya_mcu_t              dev;               /*!< TUYA MCU dev handle */
    TaskHandle_t            tsk_hdl;           /*!< task handle */
    esp_event_loop_handle_t event_loop_hdl;    /*!< Event loop handle */
    QueueHandle_t           event_queue;       /*!< UART event queue handle */
    QueueHandle_t           wifi_status_queue; /*!< WiFi send queue handle */
    QueueHandle_t           dp_queue;          /*!< DP send queue handle */
} esp_tuya_mcu_t;

/* Platform functions */
int tuya_mcu_uart_rx(void *ctx, uint8_t *c)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)ctx;
    return uart_read_bytes(mcu->uart_port, c, 1, pdMS_TO_TICKS(100));
}

int tuya_mcu_uart_tx(void *ctx, uint8_t c)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)ctx;
    return uart_write_bytes(mcu->uart_port, (const char *)&c, 1);
}

uint32_t tuya_mcu_get_tick(void)
{
    return (uint32_t)((uint64_t)xTaskGetTickCount() * (1000ULL / configTICK_RATE_HZ));
}

static void esp_tuya_mcu_task_entry(void *arg)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)arg;
    uart_event_t    event;
    tuya_dp_t       dp;
    uint8_t         wifi_state;

    ESP_LOGI(TAG, "task started on UART%d", mcu->uart_port);
    while (1) {
        if (xQueueReceive(mcu->event_queue, &event, pdMS_TO_TICKS(200))) {
            switch (event.type) {
            case UART_DATA:
                break;
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "HW FIFO Overflow");
                uart_flush(mcu->uart_port);
                xQueueReset(mcu->event_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Ring Buffer Full");
                uart_flush(mcu->uart_port);
                xQueueReset(mcu->event_queue);
                break;
            case UART_PARITY_ERR:
                ESP_LOGE(TAG, "Parity Error");
                break;
            case UART_FRAME_ERR:
                ESP_LOGE(TAG, "Frame Error");
                break;
#ifndef CONFIG_IDF_TARGET_ESP8266
            case UART_PATTERN_DET:
                break;
            case UART_BREAK:
                ESP_LOGW(TAG, "Rx Break");
                break;
#endif
            default:
                ESP_LOGW(TAG, "unknown uart event type: %d", event.type);
                break;
            }
        }

        if (xQueueReceive(mcu->wifi_status_queue, &wifi_state, pdMS_TO_TICKS(200))) {
            tuya_mcu_send_wifi_status(mcu->dev, wifi_state);
            ESP_LOGI(TAG, "WiFi status %d sent", wifi_state);
        }

        if (xQueueReceive(mcu->dp_queue, &dp, pdMS_TO_TICKS(200))) {
            tuya_mcu_send_dp(mcu->dev, &dp);
            ESP_LOGI(TAG, "DP sent: ID=%d, Type=%d, Len=%d", dp.id, dp.type, tuya_dp_get_len(&dp));
        }

        tuya_mcu_tick(mcu->dev);

        /* Drive the event loop */
        esp_event_loop_run(mcu->event_loop_hdl, pdMS_TO_TICKS(50));
    }
    vTaskDelete(NULL);
}

static int on_state_changed(tuya_mcu_t dev, enum tuya_mcu_state st, void *arg)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)arg;

    switch (st) {
    case TUYA_MCU_INIT_HEARTBEAT:
        break;
    case TUYA_MCU_QUERY_INFO:
        ESP_LOGI(TAG, "dev info queried");
        break;
    case TUYA_MCU_INITIALIZED:
        ESP_LOGI(TAG, "device initialized: ID=%s, ver=%s\n", tuya_mcu_get_product_id(dev),
                 tuya_mcu_get_version(dev));
        break;
    default:
        ESP_LOGE(TAG, "unknown state: %d\n", st);
        break;
    }
    esp_event_post_to(mcu->event_loop_hdl, TUYA_MCU_EVENT, TUYA_MCU_EVENT_STATE_CHANGED, &st,
                      sizeof(st), pdMS_TO_TICKS(100));
    return 0;
}

static int on_config_request(tuya_mcu_t dev, void *arg)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)arg;

    ESP_LOGI(TAG, "receved config request");
    esp_event_post_to(mcu->event_loop_hdl, TUYA_MCU_EVENT, TUYA_MCU_EVENT_CONFIG_REQUEST, NULL, 0,
                      pdMS_TO_TICKS(100));
    return 0;
}

static int on_dp_received(tuya_mcu_t dev, tuya_dp_t *dp, void *arg)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)arg;
    size_t          len = tuya_dp_get_len(dp);
    return esp_event_post_to(mcu->event_loop_hdl, TUYA_MCU_EVENT, TUYA_MCU_EVENT_DP_UPDATE, dp, len,
                             pdMS_TO_TICKS(100));
}

esp_tuya_mcu_handle_t esp_tuya_mcu_init(const tuya_mcu_uart_config_t *config)
{
    esp_tuya_mcu_t *mcu = calloc(1, sizeof(esp_tuya_mcu_t));
    if (!mcu) {
        ESP_LOGE(TAG, "calloc failed");
        goto err_malloc;
    }

    mcu->wifi_status_queue = xQueueCreate(4, sizeof(uint8_t));
    if (!mcu->wifi_status_queue) {
        ESP_LOGE(TAG, "create WiFi status queue failed");
        goto err_wifi_state_queue;
    }

    mcu->dp_queue = xQueueCreate(8, sizeof(tuya_dp_t));
    if (!mcu->dp_queue) {
        ESP_LOGE(TAG, "create DP queue failed");
        goto err_dp_queue;
    }

    /* Set attributes */
    mcu->uart_port = config->uart.uart_port;
    /* Install UART driver */
    uart_config_t uart_config = {
        .baud_rate = config->uart.baud_rate,
        .data_bits = config->uart.data_bits,
        .parity = config->uart.parity,
        .stop_bits = config->uart.stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#ifndef CONFIG_IDF_TARGET_ESP8266
        .source_clk = UART_SCLK_DEFAULT,
#endif
    };
    if (uart_driver_install(mcu->uart_port, RX_BUFFER_SIZE, TX_BUFFER_SIZE,
                            config->uart.event_queue_size, &mcu->event_queue, 0) != ESP_OK) {
        ESP_LOGE(TAG, "install uart driver failed");
        goto err_uart_install;
    }
    if (uart_param_config(mcu->uart_port, &uart_config) != ESP_OK) {
        ESP_LOGE(TAG, "config uart parameter failed");
        goto err_uart_config;
    }

#ifndef CONFIG_IDF_TARGET_ESP8266
    if (uart_set_pin(mcu->uart_port, config->uart.tx_pin, config->uart.rx_pin, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "config uart gpio failed");
        goto err_uart_config;
    }
#endif
    uart_flush(mcu->uart_port);

    if (tuya_mcu_init(&mcu->dev, mcu) != 0) {
        ESP_LOGE(TAG, "tuya_mcu_init failed");
        goto err_tuya_mcu;
    }

    tuya_mcu_set_state_handler(mcu->dev, on_state_changed, mcu);
    tuya_mcu_set_config_handler(mcu->dev, on_config_request, mcu);
    tuya_mcu_set_dp_handler(mcu->dev, on_dp_received, mcu);

    /* Create Event loop */
    esp_event_loop_args_t loop_args = { .queue_size = TUYA_MCU_EVENT_LOOP_QUEUE_SIZE,
                                        .task_name = NULL };
    if (esp_event_loop_create(&loop_args, &mcu->event_loop_hdl) != ESP_OK) {
        ESP_LOGE(TAG, "create event loop failed");
        goto err_eloop;
    }
    /* Create task */
    BaseType_t err = xTaskCreate(esp_tuya_mcu_task_entry, "tuya_mcu_task", TUYA_MCU_TASK_STACK_SIZE,
                                 mcu, TUYA_MCU_TASK_PRIORITY, &mcu->tsk_hdl);
    if (err != pdTRUE) {
        ESP_LOGE(TAG, "task create failed");
        goto err_task_create;
    }
    ESP_LOGI(TAG, "init OK");
    return mcu;
/*Error Handling*/
err_task_create:
    esp_event_loop_delete(mcu->event_loop_hdl);
err_eloop:
    tuya_mcu_deinit(mcu->dev);
err_tuya_mcu:
err_uart_install:
    uart_driver_delete(mcu->uart_port);
err_uart_config:
    vQueueDelete(mcu->dp_queue);
err_dp_queue:
    vQueueDelete(mcu->wifi_status_queue);
err_wifi_state_queue:
err_malloc:
    free(mcu);
    return NULL;
}

esp_err_t esp_tuya_mcu_deinit(esp_tuya_mcu_handle_t mcu_hdl)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)mcu_hdl;
    vTaskDelete(mcu->tsk_hdl);
    esp_event_loop_delete(mcu->event_loop_hdl);
    tuya_mcu_deinit(mcu->dev);
    esp_err_t err = uart_driver_delete(mcu->uart_port);
    vQueueDelete(mcu->dp_queue);
    vQueueDelete(mcu->wifi_status_queue);
    free(mcu);
    return err;
}

esp_err_t esp_tuya_mcu_add_handler(esp_tuya_mcu_handle_t mcu_hdl, esp_event_handler_t handler,
                                   void *args)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)mcu_hdl;
    return esp_event_handler_register_with(mcu->event_loop_hdl, TUYA_MCU_EVENT, ESP_EVENT_ANY_ID,
                                           handler, args);
}

esp_err_t esp_tuya_mcu_remove_handler(esp_tuya_mcu_handle_t mcu_hdl, esp_event_handler_t handler)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)mcu_hdl;
    return esp_event_handler_unregister_with(mcu->event_loop_hdl, TUYA_MCU_EVENT, ESP_EVENT_ANY_ID,
                                             handler);
}

esp_err_t esp_tuya_mcu_write_wifi_status(esp_tuya_mcu_handle_t mcu_hdl, uint8_t status)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)mcu_hdl;
    if (!mcu) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xQueueSend(mcu->wifi_status_queue, &status, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "send WiFi status to queue failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_tuya_mcu_write_dp(esp_tuya_mcu_handle_t mcu_hdl, tuya_dp_t *dp)
{
    esp_tuya_mcu_t *mcu = (esp_tuya_mcu_t *)mcu_hdl;
    if (!mcu || !dp) {
        return ESP_ERR_INVALID_ARG;
    }
    if (xQueueSend(mcu->dp_queue, dp, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "send DP to queue failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}
