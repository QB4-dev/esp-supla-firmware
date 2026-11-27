/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_BINARY_SENSOR_H_
#define _SUPLA_BINARY_SENSOR_H_

#include <esp-supla.h>
#include <driver/gpio.h>

/**
 * @brief Configuration for a GPIO-based Supla binary sensor channel.
 */
struct binary_sensor_config {
    gpio_num_t   gpio;                /**< GPIO used as binary input. */
    int          default_function;    /**< Default SUPLA_CHANNELFNC_* function. */
    unsigned int supported_functions; /**< Allowed SUPLA_BIT_FUNC_* function flags. */
};

/**
 * @brief Create a binary sensor channel instance.
 *
 * @param config Binary sensor configuration; must not be NULL.
 * @return Created channel instance on success, or NULL on allocation/init failure.
 */
supla_channel_t *supla_binary_sensor_create(const struct binary_sensor_config *config);

/**
 * @brief Delete a binary sensor channel instance and release resources.
 *
 * @param ch Channel instance to delete.
 * @return Result from supla_channel_free.
 */
int supla_binary_sensor_delete(supla_channel_t *ch);

/**
 * @brief Read current local GPIO state (with inversion logic applied).
 *
 * @param ch Channel instance.
 * @return 0 or 1 for current input state.
 */
int supla_binary_sensor_get_local(supla_channel_t *ch);

#endif /* _SUPLA_CLICK_INPUT_H_ */
