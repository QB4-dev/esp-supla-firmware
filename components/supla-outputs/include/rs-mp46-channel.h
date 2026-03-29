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

/**
 * @brief Manual movement commands supported by MP46-WiFi roller shutter MCU.
 */
typedef enum {
    MP46_RS_MANUAL_CLOSE = 0, /**< Close direction command. */
    MP46_RS_MANUAL_STOP = 1,  /**< Stop movement command. */
    MP46_RS_MANUAL_OPEN = 2,  /**< Open direction command. */
} mp46_rs_manual_cmd_t;

/**
 * @brief Configuration for MP46-WiFi UART-based roller shutter channel.
 */
struct mp46_rs_channel_config {
    tuya_mcu_uart_config_t uart; /**< Tuya MCU UART transport configuration. */
};

/**
 * @brief Create MP46-WiFi roller shutter channel instance.
 *
 * @param conf Channel configuration; must not be NULL.
 * @return Created channel instance on success, or NULL on error.
 */
supla_channel_t *supla_mp46_rs_channel_create(const struct mp46_rs_channel_config *conf);

/**
 * @brief Delete MP46-WiFi roller shutter channel instance and release resources.
 *
 * @param ch Channel instance to delete.
 * @return Result from supla_channel_free.
 */
int supla_mp46_rs_channel_delete(supla_channel_t *ch);

/**
 * @brief Update Wi-Fi status reported to Tuya MCU.
 *
 * @param ch Channel instance.
 * @param status Tuya Wi-Fi status code.
 * @return ESP_OK on success, or an ESP-IDF/Tuya error code on failure.
 */
int supla_mp46_rs_channel_set_wifi_status(supla_channel_t *ch, uint8_t status);

/**
 * @brief Send manual open/stop/close command.
 *
 * @param ch Channel instance.
 * @param cmd Manual control command enum.
 * @return ESP_OK on success, or an ESP-IDF/Tuya error code on failure.
 */
int supla_mp46_rs_channel_manual_ctrl(supla_channel_t *ch, mp46_rs_manual_cmd_t cmd);

/**
 * @brief Set target roller shutter position.
 *
 * @param ch Channel instance.
 * @param target Target position in percent (0-100).
 * @return ESP_OK on success, or an ESP-IDF/Tuya error code on failure.
 */
int supla_mp46_rs_channel_set_target_position(supla_channel_t *ch, int8_t target);

/**
 * @brief Configure motor direction mapping.
 *
 * @param ch Channel instance.
 * @param inverted true to invert direction, false for normal direction.
 * @return ESP_OK on success, or an ESP-IDF/Tuya error code on failure.
 */
int supla_mp46_rs_channel_set_direction(supla_channel_t *ch, bool inverted);

/**
 * @brief Enable or disable pull-to-start behavior in Tuya MCU.
 *
 * @param ch Channel instance.
 * @param enabled true to enable pull-to-start, false to disable.
 * @return ESP_OK on success, or an ESP-IDF/Tuya error code on failure.
 */
int supla_mp46_rs_channel_set_pull_to_start(supla_channel_t *ch, bool enabled);
#endif /* _SUPLA_MP46WIFI_CHANNEL_H_ */
