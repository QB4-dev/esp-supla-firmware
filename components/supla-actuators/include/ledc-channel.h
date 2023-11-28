/*
 * dht.h
 *
 *  Created on: 31 sty 2023
 *      Author: kuba
 */

#ifndef _SUPLA_LEDC_CHANNEL_H_
#define _SUPLA_LEDC_CHANNEL_H_

#include <libsupla/channel.h>
#include <driver/gpio.h>

supla_channel_t *supla_ledc_channel_create(gpio_num_t gpio, uint32_t fade_time);

int supla_ledc_channel_set_brightness(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);

#endif /* _SUPLA_SENSORS_DHT_H_ */
