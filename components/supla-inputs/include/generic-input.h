/*
 * dht.h
 *
 *  Created on: 31 sty 2023
 *      Author: kuba
 */

#ifndef _SUPLA_CLICK_INPUT_H_
#define _SUPLA_CLICK_INPUT_H_

#include <libsupla/channel.h>
#include <driver/gpio.h>

typedef enum {
    INPUT_EVENT_INIT = 0,
    INPUT_EVENT_CLICKx1,
    INPUT_EVENT_CLICKx2,
    INPUT_EVENT_CLICKx3,
    INPUT_EVENT_CLICKx4,
    INPUT_EVENT_CLICKx5,
    INPUT_EVENT_DONE
} input_event_t;

typedef void (*on_input_calback_t)(input_event_t event, void *);

struct generic_input_config {
    gpio_num_t          gpio;
    unsigned int        action_trigger_caps;
    supla_channel_t   **related_channel;
    on_input_calback_t on_detect_cb;
    void               *arg;
};

supla_channel_t *supla_generic_input_create(const struct generic_input_config *input_conf);

#endif /* _SUPLA_CLICK_INPUT_H_ */
