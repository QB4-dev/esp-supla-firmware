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

struct relay_channel_config {
    gpio_num_t gpio;
    int default_function;             //SUPLA_CHANNELFNC_*
    unsigned int supported_functions; //SUPLA_BIT_FUNC_*
};

supla_channel_t *supla_relay_channel_create(const struct relay_channel_config *config);

#endif /* _SUPLA_RELAY_CHANNEL_H_ */
