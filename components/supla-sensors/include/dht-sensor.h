/*
 * dht.h
 *
 *  Created on: 31 sty 2023
 *      Author: kuba
 */

#ifndef _SUPLA_SENSORS_DHT_H_
#define _SUPLA_SENSORS_DHT_H_

#include <libsupla/channel.h>
#include <dht.h>

supla_channel_t *supla_channel_dht_create(dht_sensor_type_t sensor_type, gpio_num_t gpio, uint32_t pool_interval);

#endif /* _SUPLA_SENSORS_DHT_H_ */
