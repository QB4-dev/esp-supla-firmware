/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_PCA9632_RGBW_CHANNEL_H_
#define _SUPLA_PCA9632_RGBW_CHANNEL_H_

#include <libsupla/channel.h>
#include <pca9632.h>

/**
 * @brief Output channel order mapping for RGBW mode.
 *
 * Each value defines how logical R/G/B/W channels are routed to PCA9632 LED0..LED3 outputs.
 */
typedef enum {
    RGBW_MAP_RGBW = 0,
    RGBW_MAP_GBRW = 1,
    RGBW_MAP_GRBW = 2,
    RGBW_MAP_GRWB = 3,
    RGBW_MAP_BRGW = 4,
} rgbw_mapping_t;

/**
 * @brief Warm/Cold white output order mapping for CCT mode.
 *
 * W = warm white, C = cold white; order maps values to PCA9632 LED0..LED3 outputs.
 */
typedef enum {
    CCT_MAP_WWCC = 0,
    CCT_MAP_WCWC = 1,
    CCT_MAP_CWCW = 2,
    CCT_MAP_CCWW = 3,
} cct_mapping_t;

/**
 * @brief Configuration for RGBW/CCT channel on PCA9632.
 */
struct pca9632_rgbw_channel_config {
    i2c_dev_t     *pca9632;  /**< Initialized PCA9632 I2C device handle. */
    rgbw_mapping_t rgbw_map; /**< RGBW output routing map for RGB mode. */
    cct_mapping_t  cct_map;  /**< Warm/cold output routing map for CCT mode. */
};

/**
 * @brief Configuration for a single dimmer channel on one PCA9632 output.
 */
struct pca9632_dimmer_channel_config {
    i2c_dev_t    *pca9632;    /**< Initialized PCA9632 I2C device handle. */
    pca9632_led_t pwm_output; /**< PCA9632 LED output used as dimmer PWM. */
};

/**
 * @brief Create a Supla RGBW/CCT channel controlled by PCA9632.
 *
 * @param conf RGBW/CCT channel configuration; must not be NULL.
 * @return Created channel instance on success, or NULL on error.
 */
supla_channel_t *pca9632_rgbw_channel_create(const struct pca9632_rgbw_channel_config *conf);

/**
 * @brief Create a Supla dimmer channel controlled by a single PCA9632 PWM output.
 *
 * @param conf Dimmer channel configuration; must not be NULL.
 * @return Created channel instance on success, or NULL on error.
 */
supla_channel_t *pca9632_dimmer_channel_create(const struct pca9632_dimmer_channel_config *conf);

/**
 * @brief Apply a new Supla channel value to PCA9632 channel instance.
 *
 * @param ch Target channel instance.
 * @param new_value New channel value payload provided by Supla.
 * @return ESP_OK on success, or an ESP-IDF error code on failure.
 */
int pca9632_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);

#endif /* _SUPLA_PCA9632_RGBW_CHANNEL_H_ */
