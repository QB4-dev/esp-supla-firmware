/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_SENSORS_DHT_H_
#define _SUPLA_SENSORS_DHT_H_

#include <libsupla/channel.h>
#include <dht.h>

struct dht_channel_config {
    dht_sensor_type_t sensor_type;
    gpio_num_t        gpio;
    uint32_t          poll_interval_ms;
};

supla_channel_t *supla_channel_dht_create(const struct dht_channel_config *config);

#endif /* _SUPLA_SENSORS_DHT_H_ */
