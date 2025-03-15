/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef MAIN_BSP_BOARD_H_
#define MAIN_BSP_BOARD_H_

#include <esp_err.h>
#include <device.h>
#include <settings.h>
#include <esp-supla.h>

typedef struct {
    const char             *id;
    const char             *ver;
    const settings_group_t *settings_pack;
} bsp_t;

extern bsp_t *const bsp;

esp_err_t board_early_init(void);
esp_err_t board_supla_init(supla_dev_t *dev);

esp_err_t board_on_config_mode_init(void);
esp_err_t board_on_config_mode_exit(void);

esp_err_t board_on_update_init(void);
esp_err_t board_on_update_completed(void);
esp_err_t board_on_update_fail(void);

#endif /* MAIN_BSP_BOARD_H_ */
