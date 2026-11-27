/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_STATUS_LED_RGB_H_
#define _SUPLA_STATUS_LED_RGB_H_

#include <sdkconfig.h>

#if !defined(CONFIG_IDF_TARGET_ESP8266)

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>
#include <driver/gpio.h>
#include <color.h>
#include <led_strip.h>

#include <libsupla/device.h>

/**
 * @brief Status LED instance handle.
 */
typedef void *supla_status_led_rgb_t;

/**
 * @brief Configuration for Supla device status LED driver.
 */
struct status_led_rgb_config {
    gpio_num_t           gpio;         /**< GPIO used to drive status LED output. */
    led_model_t          type;         /**< Type of LED strip. */
    led_color_component_format_t pixel_format; /**< Color channel order of the LED. */
    rgb_t                color;        /**< Color of the RGB LED (green if zero-initialized). */
    bool             online_set; /**< LED steady state in ONLINE mode (true: ON, false: OFF). */
};

/**
 * @brief Create and start a status LED instance bound to Supla device state.
 *
 * @param dev Supla device instance used to read current device state.
 * @param config Status LED configuration; must not be NULL.
 * @return Status LED handle on success, or NULL on allocation/init failure.
 */
supla_status_led_rgb_t supla_status_led_rgb_init(supla_dev_t                        *dev,
                                                 const struct status_led_rgb_config *config);

/**
 * @brief Set a custom RGB blink pattern and stop automatic Supla-state indication.
 *
 * @param led Status LED handle returned by supla_status_led_rgb_init.
 * @param color RGB color to display during the on interval.
 * @param on_ms Time in milliseconds to keep the LED on; zero means always off.
 * @param off_ms Time in milliseconds to keep the LED off; zero means always on.
 * @return ESP_OK on success.
 */
esp_err_t supla_status_led_rgb_set(supla_status_led_rgb_t led, rgb_t color, uint16_t on_ms,
                                   uint16_t off_ms);

/**
 * @brief Resume automatic indication of the current Supla device state.
 *
 * @param led Status LED handle returned by supla_status_led_rgb_init.
 * @return ESP_OK on success.
 */
esp_err_t supla_status_led_rgb_enable_state_indication(supla_status_led_rgb_t led);

/**
 * @brief Stop and delete a status LED instance.
 *
 * @param led Status LED handle returned by supla_status_led_rgb_init.
 * @return ESP_OK on success.
 */
int supla_status_led_rgb_delete(supla_status_led_rgb_t led);

#ifdef __cplusplus
}
#endif

#endif /* !CONFIG_IDF_TARGET_ESP8266 */

#endif /* _SUPLA_STATUS_LED_RGB_H_ */
