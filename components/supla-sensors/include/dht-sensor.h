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

/**
 * @brief Delete a DHT sensor channel and release its polling timer and data.
 *
 * @param ch DHT channel instance to delete.
 * @return Result from supla_channel_free, or an ESP error on failure.
 */
int supla_channel_dht_delete(supla_channel_t *ch);

/**
 * @brief Get humidity and temperature from the most recent successful poll.
 *
 * This returns cached values without reading the sensor. It returns
 * ESP_ERR_INVALID_STATE until the first successful poll.
 *
 * @param ch DHT channel instance.
 * @param humid Receives relative humidity in percent.
 * @param temp Receives temperature in degrees Celsius.
 * @return ESP_OK on success, or an ESP error if no reading is available.
 */
esp_err_t supla_channel_dht_get_local(supla_channel_t *ch, float *humid, float *temp);

#endif /* _SUPLA_SENSORS_DHT_H_ */
