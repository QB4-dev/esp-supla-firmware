/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_CLICK_INPUT_H_
#define _SUPLA_CLICK_INPUT_H_

#include <esp-supla.h>
#include <driver/gpio.h>

typedef enum {
    INPUT_EVENT_INIT = 0,
    INPUT_EVENT_CLICK1,
    INPUT_EVENT_CLICK2,
    INPUT_EVENT_CLICK3,
    INPUT_EVENT_CLICK4,
    INPUT_EVENT_CLICK5,
    INPUT_EVENT_DONE
} input_event_t;

typedef void (*on_input_calback_t)(gpio_num_t pin_num, input_event_t event, void *);

struct generic_input_config {
    gpio_num_t         gpio;
    unsigned int       action_trigger_caps;
    supla_channel_t  **related_channel;
    on_input_calback_t on_event_cb;
    void              *arg;
};

supla_channel_t *supla_generic_input_create(const struct generic_input_config *input_conf);

#endif /* _SUPLA_CLICK_INPUT_H_ */
