/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_RELAY_CHANNEL_H_
#define _SUPLA_RELAY_CHANNEL_H_

#include <libsupla/channel.h>
#include <driver/gpio.h>

/**
 * @brief Bitmask of relay functions supported by this channel implementation.
 */
#define RELAY_CH_SUPPORTED_FUNC_BITS                                                    \
    (SUPLA_BIT_FUNC_CONTROLLINGTHEGATEWAYLOCK | SUPLA_BIT_FUNC_CONTROLLINGTHEDOORLOCK | \
     SUPLA_BIT_FUNC_CONTROLLINGTHEGATE | SUPLA_BIT_FUNC_CONTROLLINGTHEGARAGEDOOR |      \
     SUPLA_BIT_FUNC_POWERSWITCH | SUPLA_BIT_FUNC_LIGHTSWITCH | SUPLA_BIT_FUNC_STAIRCASETIMER)

/**
 * @brief Configuration for a GPIO-based Supla relay channel.
 */
struct relay_channel_config {
    gpio_num_t   gpio;                /**< GPIO used to drive relay state (0=OFF, 1=ON). */
    unsigned int supported_functions; /**< Allowed SUPLA_BIT_FUNC_* function flags. */
    int          default_function;    /**< Default SUPLA_CHANNELFNC_* active function. */
};

/**
 * @brief Create a relay channel instance.
 *
 * @param conf Relay channel configuration; must not be NULL.
 * @return Created channel instance on success, or NULL on validation/allocation error.
 */
supla_channel_t *supla_relay_channel_create(const struct relay_channel_config *conf);

/**
 * @brief Destroy a relay channel instance and free associated resources.
 *
 * @param ch Channel instance to delete.
 * @return Result from supla_channel_free.
 */
int supla_relay_channel_delete(supla_channel_t *ch);

/**
 * @brief Get current relay GPIO state.
 *
 * @param ch Channel instance.
 * @return Current GPIO level (0 or 1).
 */
int supla_relay_channel_get_state(supla_channel_t *ch);

/**
 * @brief Set relay state from local logic (outside Supla server command path).
 *
 * For staircase-timer function this helper applies configured timer duration when turning ON.
 *
 * @param ch Channel instance.
 * @param relay_value Relay value payload to apply.
 * @return ESP_OK on success, or an ESP-IDF/Supla error code on failure.
 */
int supla_relay_channel_set_local(supla_channel_t *ch, TRelayChannel_Value *relay_value);

#endif /* _SUPLA_RELAY_CHANNEL_H_ */
