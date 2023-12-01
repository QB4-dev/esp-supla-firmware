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

esp_err_t webserver_init(supla_dev_t **dev);

#endif /* MAIN_WEBSERVER_H_ */
