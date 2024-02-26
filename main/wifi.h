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
#include <esp_event.h>

esp_err_t wifi_init(esp_event_handler_t eh);
bool      wifi_sta_configured(void);

esp_err_t wifi_set_station_mode(void);
esp_err_t wifi_set_access_point_mode(const char *ap_ssid);

#endif /* MAIN_WIFI_H_ */
