/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_BINARY_SENSOR_H_
#define _SUPLA_BINARY_SENSOR_H_

#include <esp-supla.h>
#include <driver/gpio.h>

struct binary_sensor_config {
    gpio_num_t   gpio;
    int          default_function;    //SUPLA_CHANNELFNC_*
    unsigned int supported_functions; //SUPLA_BIT_FUNC_*
};

supla_channel_t *supla_binary_sensor_create(const struct binary_sensor_config *config);
int              supla_binary_sensor_delete(supla_channel_t *ch);

int supla_binary_sensor_get_local(supla_channel_t *ch);

#endif /* _SUPLA_CLICK_INPUT_H_ */
