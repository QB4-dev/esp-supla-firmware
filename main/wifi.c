/*
 * wifi.c
 *
 *  Created on: 8 wrz 2022
 *      Author: kuba
 */

#include "wifi.h"

#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define CHECK_ARG(VAL) do { if (!(VAL)) return ESP_ERR_INVALID_ARG; } while (0)

static const char *TAG = "WiFi";

esp_err_t generate_hostname(const char *base, char* buf, size_t len)
{
    CHECK_ARG(base);
    CHECK_ARG(buf);

    uint8_t mac[6];

    esp_efuse_mac_get_default(mac);
    if(strlen(base) + 5 > len)
        return ESP_ERR_INVALID_SIZE;

    snprintf(buf,len,"%s-%02X%02X",base,mac[4],mac[5]);
    return ESP_OK;
}

esp_err_t wifi_set_station_mode(void)
{
	ESP_ERROR_CHECK(esp_wifi_stop());
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_connect());
	ESP_LOGI(TAG, "init station mode");
	return ESP_OK;
}

esp_err_t wifi_set_access_point_mode(const char *ap_ssid)
{
	wifi_config_t wifi_config = {
		.ap = {
			.max_connection = 1,
			.authmode = WIFI_AUTH_OPEN,
			.password = "",
		},
	};

    generate_hostname(ap_ssid,(char*)wifi_config.ap.ssid,sizeof(wifi_config.ap.ssid));
    wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);

    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA) );
    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    ESP_ERROR_CHECK(esp_wifi_start());
    //ESP_ERROR_CHECK(esp_wifi_connect());
	ESP_LOGI(TAG, "mode AP STA with AP SSID=%s",wifi_config.ap.ssid);
	return ESP_OK;
}
