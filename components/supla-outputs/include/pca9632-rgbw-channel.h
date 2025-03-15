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

typedef enum {
    RGBW_MAP_RGBW = 0,
    RGBW_MAP_GBRW = 1,
    RGBW_MAP_GRBW = 2,
    RGBW_MAP_GRWB = 3,
} rgbw_mapping_t;

struct pca9632_rgbw_channel_config {
    i2c_dev_t     *pca9632;
    rgbw_mapping_t rgbw_map;
};

struct pca9632_dimmer_channel_config {
    i2c_dev_t    *pca9632;
    pca9632_led_t pwm_output;
};

supla_channel_t *pca9632_rgbw_channel_create(const struct pca9632_rgbw_channel_config *ch_conf);
supla_channel_t *pca9632_dimmer_channel_create(const struct pca9632_dimmer_channel_config *config);

int pca9632_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);

#endif /* _SUPLA_RGBW_CHANNEL_H_ */
