/*
 * dht.h
 *
 *  Created on: 31 sty 2023
 *      Author: kuba
 */

#ifndef _SUPLA_EXT_RGBW_CHANNEL_H_
#define _SUPLA_EXT_RGBW_CHANNEL_H_

#include <libsupla/channel.h>
#include <pca9632.h>

struct ext_rgbw_channel_config {
    i2c_dev_t *pca9632;
};

supla_channel_t *supla_ext_rgbw_channel_create(const struct ext_rgbw_channel_config *ch_conf);

int supla_rgbw_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);

#endif /* _SUPLA_RGBW_CHANNEL_H_ */
