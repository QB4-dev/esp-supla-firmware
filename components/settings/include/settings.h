/*
 * settings.h
 *
 *  Created on: 3 lut 2024
 *      Author: kuba
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <esp_http_server.h>
#include <esp_err.h>

#include "settings-defs.h"

void settings_pack_print(const settings_group_t *group);
setting_t *settings_pack_find(const settings_group_t *settings_pack, const char *group, const char *label);

esp_err_t settings_nvs_read(const settings_group_t *settings_pack);
esp_err_t settings_nvs_write(settings_group_t *settings_pack);
esp_err_t settings_nvs_erase(void);

esp_err_t settings_httpd_handler(httpd_req_t *req);

#endif /* SETTINGS_H_ */
