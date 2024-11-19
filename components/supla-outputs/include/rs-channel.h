/*
 * relay-channel.h
 *
 *  Created on: 31 sty 2023
 *      Author: kuba
 */

#ifndef _SUPLA_RELAY_CHANNEL_H_
#define _SUPLA_RELAY_CHANNEL_H_

#include <libsupla/channel.h>
#include <driver/gpio.h>

#define RS_CH_SUPPORTED_FUNC_BITS                                                               \
    (SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER | SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW |     \
     SUPLA_BIT_FUNC_TERRACE_AWNING | SUPLA_BIT_FUNC_PROJECTOR_SCREEN | SUPLA_BIT_FUNC_CURTAIN | \
     SUPLA_BIT_FUNC_VERTICAL_BLIND | SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR)

struct rs_channel_config {
    gpio_num_t   gpio_fwd;
    gpio_num_t   gpio_bck;
    unsigned int supported_functions; //SUPLA_BIT_FUNC_*
    int          default_function;    //SUPLA_CHANNELFNC_*
};

supla_channel_t *supla_rs_channel_create(const struct rs_channel_config *config);

#endif /* _SUPLA_RELAY_CHANNEL_H_ */
