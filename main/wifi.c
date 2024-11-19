/*
 * wifi.c
 *
 *  Created on: 8 wrz 2022
 *      Author: kuba
 */

#include "wifi.h"

#include <string.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "lwip/err.h"
#include "lwip/sys.h"

#define CHECK_ARG(VAL)                  \
    do {                                \
        if (!(VAL))                     \
            return ESP_ERR_INVALID_ARG; \
    } while (0)

static const char *TAG = "WiFi";

esp_err_t wifi_init(esp_event_handler_t eh)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, eh, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, eh, NULL));

#if CONFIG_IDF_TARGET_ESP32
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
#endif

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}

esp_err_t wifi_set_station_mode(void)
{
    ESP_LOGI(TAG, "set station mode");
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    return ESP_OK;
}

esp_err_t wifi_set_access_point_mode(const char *ap_ssid)
{
    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 1,
            .authmode = WIFI_AUTH_OPEN,
        },
    };

    strncpy((char *)wifi_config.ap.ssid, ap_ssid, sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);

    ESP_LOGI(TAG, "set AP mode with SSID: %s", wifi_config.ap.ssid);
    esp_wifi_disconnect();
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}
