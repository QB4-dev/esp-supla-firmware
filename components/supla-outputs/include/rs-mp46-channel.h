/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_MP46WIFI_CHANNEL_H_
#define _SUPLA_MP46WIFI_CHANNEL_H_

#include <esp-supla.h>
#include <driver/uart.h>

#include "esp-tuya-mcu.h"

typedef enum {
    MP46_RS_MANUAL_CLOSE = 0,
    MP46_RS_MANUAL_STOP = 1,
    MP46_RS_MANUAL_OPEN = 2,
} mp46_rs_manual_cmd_t;

struct mp46_rs_channel_config {
    tuya_mcu_uart_config_t uart;
};

supla_channel_t *supla_mp46_rs_channel_create(const struct mp46_rs_channel_config *config);
int              supla_mp46_rs_channel_delete(supla_channel_t *ch);

int supla_mp46_rs_channel_set_wifi_status(supla_channel_t *ch, uint8_t status);
int supla_mp46_rs_channel_manual_ctrl(supla_channel_t *ch, mp46_rs_manual_cmd_t cmd);
int supla_mp46_rs_channel_set_target_position(supla_channel_t *ch, int8_t target);

int supla_mp46_rs_channel_set_direction(supla_channel_t *ch, bool inverted);
int supla_mp46_rs_channel_set_pull_to_start(supla_channel_t *ch, bool enabled);
#endif /* _SUPLA_MP46WIFI_CHANNEL_H_ */
