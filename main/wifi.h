/*
 * wifi.h
 *
 *  Created on: 8 wrz 2022
 *      Author: kuba
 */

#ifndef MAIN_WIFI_H_
#define MAIN_WIFI_H_

#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD

esp_err_t wifi_init(system_event_cb_t event_handler);

esp_err_t wifi_set_station_mode(void);
esp_err_t wifi_set_access_point_mode(const char *ap_ssid);

#endif /* MAIN_WIFI_H_ */
