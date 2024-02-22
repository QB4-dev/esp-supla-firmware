/*
 * dht.h
 *
 *  Created on: 31 sty 2023
 *      Author: kuba
 */

#ifndef _SUPLA_RGBW_CHANNEL_H_
#define _SUPLA_RGBW_CHANNEL_H_

#include <libsupla/channel.h>

struct rgbw_channel_config {
    uint32_t fade_time; //ms
};

supla_channel_t *supla_rgbw_channel_create(const struct rgbw_channel_config *ch_conf);

int supla_rgbw_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);

#endif /* _SUPLA_RGBW_CHANNEL_H_ */
