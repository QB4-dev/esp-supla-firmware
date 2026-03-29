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

/**
 * @brief Status LED instance handle.
 */
typedef void *supla_status_led_t;

/**
 * @brief Configuration for Supla device status LED driver.
 */
struct status_led_config {
    gpio_num_t gpio;       /**< GPIO used to drive status LED output. */
    bool       inv_logic;  /**< Invert LED logic level (true: active low, false: active high). */
    bool       online_set; /**< LED steady state in ONLINE mode (true: ON, false: OFF). */
};

/**
 * @brief Create and start a status LED instance bound to Supla device state.
 *
 * @param dev Supla device instance used to read current device state.
 * @param config Status LED configuration; must not be NULL.
 * @return Status LED handle on success, or NULL on allocation/init failure.
 */
supla_status_led_t supla_status_led_init(supla_dev_t *dev, const struct status_led_config *config);

/**
 * @brief Stop and delete a status LED instance.
 *
 * @param led Status LED handle returned by supla_status_led_init.
 * @return ESP_OK on success.
 */
int supla_status_led_delete(supla_status_led_t led);

#ifdef __cplusplus
}
#endif

#endif /* _SUPLA_STATUS_LED_H_ */
