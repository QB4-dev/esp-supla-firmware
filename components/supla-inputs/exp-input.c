/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/exp-input.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stdlib.h>
#include <string.h>
#include <esp_timer.h>
#include <esp_log.h>

#define DEFAULT_POLL_INTERVAL_US 100000 //100ms

struct exp_input_data {
    i2c_dev_t             *i2c_exp;
    gpio_num_t             pin_num;
    exp_input_active_t     active_level;
    uint32_t               hold_time;
    uint32_t               prev_level;
    esp_timer_handle_t     timer;
    TickType_t             init_tick;
    bool                   hold;
    on_exp_event_calback_t callback;
    void                  *cb_arg;
};

static void exp_input_poll(void *arg)
{
    supla_channel_t       *ch = arg;
    supla_channel_config_t ch_config;
    uint32_t               level;
    TickType_t             tick = xTaskGetTickCount();
    exp_input_event_t      event = EXP_INPUT_EVENT_NONE;
    struct exp_input_data *data = supla_channel_get_data(ch);

    supla_channel_get_config(ch, &ch_config);
    pca9557_get_level(data->i2c_exp, data->pin_num, &level);
    /* falling edge */
    if (data->prev_level && !level) {
        event = (data->active_level == ACTIVE_LOW) ? EXP_INPUT_EVENT_INIT : EXP_INPUT_EVENT_DONE;
    }
    /* rising edge */
    if (!data->prev_level && level) {
        event = (data->active_level == ACTIVE_HIGH) ? EXP_INPUT_EVENT_INIT : EXP_INPUT_EVENT_DONE;
    }

    if (event == EXP_INPUT_EVENT_INIT) {
        data->init_tick = xTaskGetTickCount();
        data->hold = false;
    }

    if (level == data->active_level && !data->hold &&
        (tick - data->init_tick) > pdMS_TO_TICKS(data->hold_time)) {
        event = EXP_INPUT_EVENT_HOLD;
        data->hold = true;
    }

    switch (event) {
    case EXP_INPUT_EVENT_INIT:
        if (ch_config.action_trigger_caps & SUPLA_ACTION_CAP_TURN_ON)
            supla_channel_emit_action(ch, SUPLA_ACTION_CAP_TURN_ON);

        data->callback(data->pin_num, event, data->cb_arg);
        break;
    case EXP_INPUT_EVENT_HOLD:
        data->callback(data->pin_num, EXP_INPUT_EVENT_HOLD, data->cb_arg);
        break;
    case EXP_INPUT_EVENT_DONE:
        if (ch_config.action_trigger_caps & SUPLA_ACTION_CAP_TURN_OFF)
            supla_channel_emit_action(ch, SUPLA_ACTION_CAP_TURN_OFF);

        data->callback(data->pin_num, event, data->cb_arg);
        break;
    default:
        break;
    }
    data->prev_level = level;
}

supla_channel_t *supla_exp_input_create(const struct exp_input_config *input_conf)
{
    supla_channel_config_t at_channel_config = {
        .type = SUPLA_CHANNELTYPE_ACTIONTRIGGER,
        .supported_functions = SUPLA_CHANNELFNC_ACTIONTRIGGER,
        .default_function = SUPLA_CHANNELFNC_ACTIONTRIGGER,
        .action_trigger_caps = input_conf->action_trigger_caps,
        .action_trigger_related_channel = input_conf->related_channel
    };

    esp_timer_create_args_t timer_args = {
        .name = "exp-input",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = exp_input_poll,
    };
    struct exp_input_data *data;

    supla_channel_t *ch = supla_channel_create(&at_channel_config);
    if (!ch) {
        return NULL;
    }

    data = calloc(1, sizeof(struct exp_input_data));
    if (!data) {
        supla_channel_free(ch);
        return NULL;
    }

    data->i2c_exp = input_conf->i2c_expander;
    data->pin_num = input_conf->pin_num;
    data->callback = input_conf->callback;
    data->cb_arg = input_conf->arg;
    data->active_level = input_conf->active_level;
    data->hold_time = input_conf->hold_time ? input_conf->hold_time : 3000;
    supla_channel_set_data(ch, data);

    pca9557_set_mode(data->i2c_exp, data->pin_num, PCA9557_MODE_INPUT);
    pca9557_get_level(data->i2c_exp, data->pin_num, &data->prev_level);

    timer_args.arg = ch;
    esp_timer_create(&timer_args, &data->timer);
    esp_timer_start_periodic(data->timer, DEFAULT_POLL_INTERVAL_US);
    return ch;
}
