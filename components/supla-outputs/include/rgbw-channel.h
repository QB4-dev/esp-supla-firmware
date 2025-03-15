/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_RGBW_CHANNEL_H_
#define _SUPLA_RGBW_CHANNEL_H_

#include <libsupla/channel.h>

struct rgbw_channel_config {
    uint32_t fade_time; //ms
};

supla_channel_t *pca9632_rgbw_channel_create(const struct rgbw_channel_config *ch_conf);

int supla_rgbw_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value);

#endif /* _SUPLA_RGBW_CHANNEL_H_ */
