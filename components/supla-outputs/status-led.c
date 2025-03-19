/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/status-led.h"
#include <stdlib.h>
#include <stdbool.h>
#include <esp_timer.h>
#include <esp_log.h>

struct led_data {
    struct status_led_config conf;
    supla_dev_t             *supla_dev;
    bool                     on;
    uint16_t                 ontime, offtime;
    esp_timer_handle_t       timer;
    esp_timer_handle_t       state_timer;
};

static void led_timer_callback(void *arg)
{
    struct led_data *led = arg;

    led->on = !led->on;
    gpio_set_level(led->conf.gpio, led->on);
    esp_timer_start_periodic(led->timer, led->on ? led->ontime * 1000 : led->offtime * 1000);
}

static esp_err_t led_set_state(struct led_data *led, uint16_t ontime, uint16_t offtime)
{
    esp_timer_stop(led->timer);
    led->ontime = ontime;
    led->offtime = offtime;

    if (ontime == 0) {
        led->on = !led->conf.inv_logic ? false : true;
        gpio_set_level(led->conf.gpio, led->on);
    } else if (offtime == 0) {
        led->on = !led->conf.inv_logic ? true : false;
        gpio_set_level(led->conf.gpio, led->on);
    } else {
        led->on = !led->conf.inv_logic ? true : false;
        gpio_set_level(led->conf.gpio, led->on);
        esp_timer_start_periodic(led->timer, led->on ? led->ontime * 1000 : led->offtime * 1000);
    }
    return ESP_OK;
}

static void state_timer_callback(void *arg)
{
    struct led_data  *led = arg;
    supla_dev_state_t st;

    if (supla_dev_get_state(led->supla_dev, &st) != SUPLA_RESULT_TRUE)
        return;

    switch (st) {
    case SUPLA_DEV_STATE_CONFIG:
        led_set_state(led, 100, 100);
        break;
    case SUPLA_DEV_STATE_IDLE:
        led_set_state(led, 1000, 0);
        break;
    case SUPLA_DEV_STATE_INIT:
        led_set_state(led, 500, 500);
        break;
    case SUPLA_DEV_STATE_CONNECTED:
        led_set_state(led, 200, 200);
        break;
    case SUPLA_DEV_STATE_REGISTERED:
        led_set_state(led, 10, 10);
        break;
    case SUPLA_DEV_STATE_ONLINE:
        led_set_state(led, led->conf.online_set, 0);
        break;
    default:
        break;
    }
}

supla_status_led_t supla_status_led_init(supla_dev_t *dev, const struct status_led_config *config)
{
    struct led_data *led;

    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << config->gpio),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE //
    };
    esp_timer_create_args_t led_timer_args = {
        .name = "status-led",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = led_timer_callback,
    };
    esp_timer_create_args_t state_timer_args = {
        .name = "supla-state",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = state_timer_callback,
    };

    led = calloc(1, sizeof(struct led_data));
    if (!led) {
        return NULL;
    }

    led->supla_dev = dev;
    led->conf = *config;
    led->ontime = 0;
    led->offtime = 0;
    led_timer_args.arg = led;
    state_timer_args.arg = led;

    gpio_config(&gpio_conf);
    esp_timer_create(&led_timer_args, &led->timer);
    esp_timer_create(&state_timer_args, &led->state_timer);
    esp_timer_start_periodic(led->state_timer, 1000 * 1000);
    return led;
}

int supla_status_led_delete(supla_status_led_t led)
{
    struct led_data *data = led;

    esp_timer_delete(data->timer);
    esp_timer_delete(data->state_timer);
    free(data);
    return ESP_OK;
}
