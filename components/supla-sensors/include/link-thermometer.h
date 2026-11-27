/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_LINK_THERMOMETER_H_
#define _SUPLA_LINK_THERMOMETER_H_

#include <esp_err.h>
#include <stdint.h>
#include <libsupla/channel.h>

/**
 * @brief Configuration for a thermometer reading a SUPLA direct link.
 */
struct link_thermometer_config {
    const char *link;             /**< Direct link URL returning the sensor JSON. */
    uint32_t    poll_interval_ms; /**< Poll interval in milliseconds; 0 uses the default. */
};

/**
 * @brief Create a thermometer channel that polls a SUPLA direct link.
 *
 * The request sends an Accept: application/json header.
 *
 * @param config Link and polling configuration; must not be NULL.
 * @return Created channel on success, or NULL on failure.
 */
supla_channel_t *link_thermometer_create(const struct link_thermometer_config *config);

/**
 * @brief Read the most recently received temperature.
 *
 * @param ch Link thermometer channel.
 * @param temperature Receives temperature in degrees Celsius.
 * @return ESP_OK on success, or ESP_ERR_INVALID_STATE before the first valid response.
 */
esp_err_t link_thermometer_get_local(supla_channel_t *ch, double *temperature);

/**
 * @brief Stop polling and release a link thermometer channel.
 *
 * @param ch Channel to delete.
 * @return Result from supla_channel_free, or an ESP error on failure.
 */
int link_thermometer_delete(supla_channel_t *ch);

#endif /* _SUPLA_LINK_THERMOMETER_H_ */
