/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_SENSORS_DHT_H_
#define _SUPLA_SENSORS_DHT_H_

#include <libsupla/channel.h>
#include <dht.h>

/**
 * @brief Configuration for a DHT temperature/humidity Supla channel.
 */
struct dht_channel_config {
    dht_sensor_type_t sensor_type;      /**< DHT sensor model (for example DHT11 or DHT22). */
    gpio_num_t        gpio;             /**< GPIO connected to DHT data pin. */
    uint32_t          poll_interval_ms; /**< Polling interval in milliseconds; 0 uses default. */
};

/**
 * @brief Create a Supla humidity and temperature channel backed by DHT sensor.
 *
 * @param conf DHT channel configuration; must not be NULL.
 * @return Created channel instance on success, or NULL on allocation/init failure.
 */
supla_channel_t *supla_channel_dht_create(const struct dht_channel_config *conf);

#endif /* _SUPLA_SENSORS_DHT_H_ */
