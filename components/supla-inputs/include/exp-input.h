/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_EXP_INPUT_H_
#define _SUPLA_EXP_INPUT_H_

#include <esp-supla.h>
#include <i2cdev.h>
#include <driver/gpio.h>

#define EXP_INPUT_DEFAULT_HOLD_TIME_MS 3000

/**
 * @brief Input active level configuration for expander input.
 */
typedef enum {
    ACTIVE_LOW = 0, /**< Input is active when low. */
    ACTIVE_HIGH = 1 /**< Input is active when high. */
} exp_input_active_t;

/**
 * @brief Event types reported by expander input logic.
 */
typedef enum {
    EXP_INPUT_EVENT_NONE = 0, /**< No event. */
    EXP_INPUT_EVENT_INIT,     /**< Input activated (pressed/triggered). */
    EXP_INPUT_EVENT_HOLD,     /**< Input held for configured hold_time. */
    EXP_INPUT_EVENT_DONE      /**< Input released (deactivated). */
} exp_input_event_t;

/**
 * @brief Callback type for expander input events.
 *
 * @param pin_num GPIO number (expander pin).
 * @param event Event type (init, hold, done).
 * @param arg User-provided argument pointer.
 */
typedef void (*exp_event_callback_t)(gpio_num_t pin_num, exp_input_event_t event, void *arg);

/**
 * @brief Callback type for expander input setup.
 *
 * @param i2c_expander Initialized I2C expander device.
 * @param pin Expander pin number to configure.
 * @return ESP_OK on success, or an error code on failure.
 */
typedef esp_err_t (*exp_setup_callback_t)(i2c_dev_t *i2c_expander, uint8_t pin);

/**
 * @brief Callback type for expander input read.
 *
 * @param i2c_expander Initialized I2C expander device.
 * @param pin Expander pin number to read.
 * @param val Pointer to store the read value.
 * @return ESP_OK on success, or an error code on failure.
 */
typedef esp_err_t (*exp_read_callback_t)(i2c_dev_t *i2c_expander, uint8_t pin, uint32_t *val);

/**
 * @brief Configuration for i2c expander input channel.
 */
struct exp_input_config {
    i2c_dev_t           *i2c_expander;       /**< Initialized I2C expander device. */
    exp_setup_callback_t exp_setup_callback; /**< Setup callback function. */
    exp_read_callback_t  exp_read_callback;  /**< Read callback function. */

    gpio_num_t         pin_num;      /**< Expander pin number to use as input. */
    exp_input_active_t active_level; /**< Input active level (low or high). */
    uint32_t           hold_time;    /**< Hold time in ms for hold event; 0 uses default. */

    unsigned int      action_trigger_caps; /**< SUPLA_ACTION_CAP_* bitmask for supported actions. */
    supla_channel_t **related_channel;     /**< Related channels for action trigger. */

    exp_event_callback_t event_callback; /**< Event callback function. */
    void                *cb_arg;         /**< User argument for callback. */
};

/**
 * @brief Create an expander input channel instance.
 *
 * @param input_conf Expander input configuration; must not be NULL.
 * @return Created channel instance on success, or NULL on allocation/init failure.
 */
supla_channel_t *supla_exp_input_create(const struct exp_input_config *input_conf);

#endif /* _SUPLA_EXP_INPUT_H_ */
