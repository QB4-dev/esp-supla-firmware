/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _SUPLA_EXP_INPUT_H_
#define _SUPLA_EXP_INPUT_H_

#include <esp-supla.h>
#include <driver/gpio.h>
#include <pca9557.h>

typedef enum { ACTIVE_LOW = 0, ACTIVE_HIGH = 1 } exp_input_active_t;

typedef enum {
    EXP_INPUT_EVENT_NONE = 0,
    EXP_INPUT_EVENT_INIT,
    EXP_INPUT_EVENT_HOLD,
    EXP_INPUT_EVENT_DONE
} exp_input_event_t;

typedef void (*on_exp_event_calback_t)(gpio_num_t pin_num, exp_input_event_t event, void *);

struct exp_input_config {
    i2c_dev_t             *i2c_expander;
    gpio_num_t             pin_num;
    exp_input_active_t     active_level;
    uint32_t               hold_time;
    unsigned int           action_trigger_caps;
    supla_channel_t      **related_channel;
    on_exp_event_calback_t callback;
    void                  *arg;
};

supla_channel_t *supla_exp_input_create(const struct exp_input_config *input_conf);

#endif /* _SUPLA_EXP_INPUT_H_ */
