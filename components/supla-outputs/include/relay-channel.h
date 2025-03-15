/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_RELAY_CHANNEL_H_
#define _SUPLA_RELAY_CHANNEL_H_

#include <libsupla/channel.h>
#include <driver/gpio.h>

#define RELAY_CH_SUPPORTED_FUNC_BITS                                                    \
    (SUPLA_BIT_FUNC_CONTROLLINGTHEGATEWAYLOCK | SUPLA_BIT_FUNC_CONTROLLINGTHEDOORLOCK | \
     SUPLA_BIT_FUNC_CONTROLLINGTHEGATE | SUPLA_BIT_FUNC_CONTROLLINGTHEGARAGEDOOR |      \
     SUPLA_BIT_FUNC_POWERSWITCH | SUPLA_BIT_FUNC_LIGHTSWITCH | SUPLA_BIT_FUNC_STAIRCASETIMER)

struct relay_channel_config {
    gpio_num_t   gpio;
    unsigned int supported_functions; //SUPLA_BIT_FUNC_*
    int          default_function;    //SUPLA_CHANNELFNC_*
};

supla_channel_t *supla_relay_channel_create(const struct relay_channel_config *config);
int              supla_relay_channel_delete(supla_channel_t *ch);

int supla_relay_channel_get_state(supla_channel_t *ch);
int supla_relay_channel_set_local(supla_channel_t *ch, TRelayChannel_Value *relay_value);

#endif /* _SUPLA_RELAY_CHANNEL_H_ */
