/*
 * webserver.h
 *
 *  Created on: 28 lis 2023
 *      Author: kuba
 */

#ifndef MAIN_WEBSERVER_H_
#define MAIN_WEBSERVER_H_

#include <esp_http_server.h>
#include <esp-supla.h>
#include <settings.h>

esp_err_t webserver_start(supla_dev_t **dev);
esp_err_t webserver_stop(void);

esp_err_t webserver_use_settings(const settings_group_t *settings_pack);

#endif /* MAIN_WEBSERVER_H_ */
