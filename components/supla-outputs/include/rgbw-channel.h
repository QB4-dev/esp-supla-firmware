/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_RGBW_CHANNEL_H_
#define _SUPLA_RGBW_CHANNEL_H_

#include <libsupla/channel.h>
#include <driver/ledc.h>
#include <driver/gpio.h>

#ifndef GPIO_NUM_NC
#define GPIO_NUM_NC (-1)
#endif

/**
 * @brief Configuration for an RGBW channel using ESP-IDF LEDC PWM.
 *
 * Set gpio_ww and gpio_cw to GPIO_NUM_NC to use RGB only (no white channels).
 */
struct rgbw_channel_config {
    gpio_num_t gpio_r;    ///< GPIO for Red
    gpio_num_t gpio_g;    ///< GPIO for Green
    gpio_num_t gpio_b;    ///< GPIO for Blue
    gpio_num_t gpio_ww;   ///< GPIO for Warm White, or GPIO_NUM_NC for RGB only
    gpio_num_t gpio_cw;   ///< GPIO for Cold White, or GPIO_NUM_NC for RGB only
    uint32_t   fade_time; ///< Fade transition time in ms
};

supla_channel_t *rgbw_channel_create(const struct rgbw_channel_config *ch_conf);

int rgbw_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);

#endif /* _SUPLA_RGBW_CHANNEL_H_ */
