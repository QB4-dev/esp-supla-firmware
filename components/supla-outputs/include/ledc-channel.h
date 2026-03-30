/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_LEDC_CHANNEL_H_
#define _SUPLA_LEDC_CHANNEL_H_

#include <libsupla/channel.h>
#include <driver/ledc.h>
#include <driver/gpio.h>

/**
 * @brief Configuration for a dimmer channel implemented with ESP-IDF LEDC PWM.
 */
struct ledc_channel_config {
    gpio_num_t     gpio;         /**< GPIO used by the LEDC output channel. */
    ledc_channel_t ledc_channel; /**< LEDC channel index to configure. */
    uint32_t       fade_time;    /**< Fade transition time in milliseconds. */
};

/**
 * @brief Create a Supla dimmer channel backed by LEDC PWM output.
 *
 * @param ledc_ch_conf LEDC channel configuration; must not be NULL.
 * @return Created channel instance on success, or NULL on error.
 */
supla_channel_t *ledc_dimmer_channel_create(const struct ledc_channel_config *ledc_ch_conf);

/**
 * @brief Set current channel brightness.
 *
 * @param ch Target channel instance.
 * @param new_value Supla payload with brightness and optional duration.
 * @return ESP_OK on success, or an ESP-IDF/Supla error code on failure.
 */
int ledc_dimmer_set_brightness(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);

/**
 * @brief Read current channel brightness.
 *
 * @param ch Target channel instance.
 * @param brightness Output pointer receiving brightness in percent (0-100).
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG when brightness is NULL.
 */
int ledc_dimmer_get_brightness(supla_channel_t *ch, uint8_t *brightness);

/**
 * @brief Set base brightness value used on fade-out.
 *
 * @param ch Target channel instance.
 * @param new_value Supla payload with brightness and optional duration.
 * @return ESP_OK on success, or an ESP-IDF/Supla error code on failure.
 */
int ledc_dimmer_set_base_brightness(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);

/**
 * @brief Read configured base brightness value.
 *
 * @param ch Target channel instance.
 * @param brightness Output pointer receiving base brightness in percent (0-100).
 * @return ESP_OK on success, or ESP_ERR_INVALID_ARG when brightness is NULL.
 */
int ledc_dimmer_get_base_brightness(supla_channel_t *ch, uint8_t *brightness);

#endif /* _SUPLA_LEDC_CHANNEL_H_ */
