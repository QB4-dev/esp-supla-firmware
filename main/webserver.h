/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef MAIN_WEBSERVER_H_
#define MAIN_WEBSERVER_H_

#include <esp_http_server.h>
#include <esp-supla.h>
#include <board.h>

esp_err_t webserver_start(supla_dev_t **dev, const bsp_t *brd);
esp_err_t webserver_stop(void);

#endif /* MAIN_WEBSERVER_H_ */
