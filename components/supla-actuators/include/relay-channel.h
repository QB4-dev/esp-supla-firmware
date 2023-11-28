/*
 * dht.h
 *
 *  Created on: 31 sty 2023
 *      Author: kuba
 */

#ifndef _SUPLA_RELAY_CHANNEL_H_
#define _SUPLA_RELAY_CHANNEL_H_

#include <libsupla/channel.h>
#include <driver/gpio.h>

supla_channel_t *supla_relay_channel_create(gpio_num_t gpio);

#endif /* _SUPLA_RELAY_CHANNEL_H_ */
