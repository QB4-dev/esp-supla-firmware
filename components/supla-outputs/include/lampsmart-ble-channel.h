/*
 * Copyright (c) 2026 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_LEDSMART_BLE_CHANNEL_H_
#define _SUPLA_LEDSMART_BLE_CHANNEL_H_

#include <libsupla/channel.h>
#include <driver/gpio.h>
#include <lampsmart_ble.h>

/**
 * @brief Configuration for an RGBW channel using ESP-IDF LEDC PWM.
 *
 * Set gpio_ww and gpio_cw to GPIO_NUM_NC to use RGB only (no white channels).
 */
struct lamp_ble_channel_config {
    lampsmart_ble_config_t lamp_config;
    gpio_num_t             relay_gpio;
};

supla_channel_t *lamp_ble_channel_create(const struct lamp_ble_channel_config *ch_conf);

int lamp_ble_channel_set_protocol(supla_channel_t *ch, lampsmart_variant_t variant);
int lamp_ble_channel_set_group_id(supla_channel_t *ch, uint32_t group_id);

int lamp_ble_channel_pair(supla_channel_t *ch);
int lamp_ble_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *value);
int lamp_ble_channel_get_value(supla_channel_t *ch, TRGBW_Value *value);

#endif /* _SUPLA_LEDSMART_BLE_CHANNEL_H_ */
