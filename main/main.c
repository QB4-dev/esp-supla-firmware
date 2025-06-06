/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <esp_system.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include <esp_netif.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <device.h>
#include <board.h>

#include "wifi.h"
#include "webserver.h"

static const char *TAG = "APP";

static char         hostname[32];
static supla_dev_t *supla_dev;

static struct supla_config supla_config = {
#ifdef CONFIG_ESP_LIBSUPLA_USE_ESP_TLS
    .ssl = 1
#endif
};

static void net_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            break;
        case WIFI_EVENT_STA_CONNECTED: {
            wifi_event_sta_connected_t *info = data;

            ESP_LOGI(TAG, "Connected to SSID: %s", info->ssid);
        } break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t *info = data;

            ESP_LOGE(TAG, "Station disconnected(reason : %d)", info->reason);
            //          if (info->reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
            //              /*Switch to 802.11 bgn mode */
            //              esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
            //          }
            if (info->reason == WIFI_REASON_ASSOC_LEAVE)
                break; // disconnected by user

            /* Let the chip cool down for a while */
            vTaskDelay(pdMS_TO_TICKS(5000));
            esp_wifi_connect();
        } break;
        default:
            break;
        }
    } else if (base == IP_EVENT) {
        switch (id) {
        case IP_EVENT_STA_GOT_IP: {
            ip_event_got_ip_t *event = data;
            ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
            supla_dev_start(supla_dev);
        } break;
        default:
            break;
        }
    }
}

static void dev_event_handler(void *event_handler_arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    switch (event_id) {
    case DEVICE_EVENT_CONFIG_INIT:
        board_on_config_mode_init();
        supla_dev_enter_config_mode(supla_dev);
        wifi_set_access_point_mode(hostname);
        webserver_start(&supla_dev, bsp);
        break;
    case DEVICE_EVENT_CONFIG_EXIT: {
        board_on_config_mode_exit();
        supla_dev_exit_config_mode(supla_dev);
        webserver_stop();
        wifi_set_station_mode();
    } break;
    default:
        break;
    }
}

static void supla_dev_state_change_callback(supla_dev_t *dev, supla_dev_state_t state)
{
    supla_log(LOG_INFO, "state -> %s", supla_dev_state_str(state));

    switch (state) {
    case SUPLA_DEV_STATE_CONFIG:
        device_init_config();
        break;
    case SUPLA_DEV_STATE_IDLE:
        break;
    case SUPLA_DEV_STATE_INIT:
        break;
    case SUPLA_DEV_STATE_CONNECTED:
        break;
    case SUPLA_DEV_STATE_REGISTERED:
        break;
    case SUPLA_DEV_STATE_ONLINE:
        break;
    default:
        break;
    }
}

static esp_err_t supla_device_init(void)
{
    supla_dev = supla_dev_create(bsp->id, NULL);

    supla_esp_generate_hostname(supla_dev, hostname, sizeof(hostname));
    supla_dev_set_flags(supla_dev, SUPLA_DEVICE_FLAG_CALCFG_ENTER_CFG_MODE);
    supla_dev_set_state_changed_callback(supla_dev, supla_dev_state_change_callback);
    supla_dev_set_common_channel_state_callback(supla_dev, supla_esp_get_wifi_state);
    supla_dev_set_server_time_sync_callback(supla_dev, supla_esp_server_time_sync);
    supla_dev_set_server_req_restart_callback(supla_dev, supla_esp_restart_callback);

    supla_esp_nvs_config_init(&supla_config);
    supla_dev_set_config(supla_dev, &supla_config);
    return ESP_OK;
}

void app_main()
{
    ESP_ERROR_CHECK(board_early_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(device_init(dev_event_handler, NULL));
    ESP_ERROR_CHECK(supla_device_init());
    ESP_ERROR_CHECK(board_supla_init(supla_dev));
    ESP_ERROR_CHECK(wifi_init(net_event_handler));

    if (supla_config.email[0] == 0)
        device_init_config();
    else
        wifi_set_station_mode();

    while (1) {
        supla_dev_iterate(supla_dev);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
