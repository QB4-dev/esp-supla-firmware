/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_STATUS_LED_H_
#define _SUPLA_STATUS_LED_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <esp_err.h>
#include <driver/gpio.h>

#include <libsupla/device.h>

typedef void *supla_status_led_t;

struct status_led_config {
    gpio_num_t gpio;
    bool       inv_logic;
    bool       online_set;
};

supla_status_led_t supla_status_led_init(supla_dev_t *dev, const struct status_led_config *config);
int                supla_status_led_delete(supla_status_led_t led);

#ifdef __cplusplus
}
#endif

#endif /* _SUPLA_STATUS_LED_H_ */
