/*
 * dht.h
 *
 *  Created on: 31 sty 2023
 *      Author: kuba
 */

#ifndef _SUPLA_LEDC_CHANNEL_H_
#define _SUPLA_LEDC_CHANNEL_H_

#include <libsupla/channel.h>
#include <driver/ledc.h>
#include <driver/gpio.h>

struct ledc_channel_config {
    gpio_num_t gpio;
    ledc_channel_t ledc_channel;
    uint32_t fade_time; //ms
};

supla_channel_t *supla_ledc_channel_create(const struct ledc_channel_config *ledc_ch_conf);

int supla_ledc_channel_set_brightness(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);
int supla_ledc_channel_get_brightness(supla_channel_t *ch, uint8_t *brightness);

#endif /* _SUPLA_SENSORS_DHT_H_ */
