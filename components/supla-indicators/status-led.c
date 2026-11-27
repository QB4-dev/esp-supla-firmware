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

static const char *TAG = "status-led";

struct led_data {
    struct status_led_config conf;
    supla_dev_t             *supla_dev;
    bool                     on;
    bool                     manual;
    uint16_t                 ontime, offtime;
    esp_timer_handle_t       timer;
    esp_timer_handle_t       state_timer;
};

static esp_err_t led_write(struct led_data *led)
{
    return gpio_set_level(led->conf.gpio, led->on != led->conf.inv_logic);
}

static void led_timer_callback(void *arg)
{
    struct led_data *led = arg;

    led->on = !led->on;
    esp_err_t err = led_write(led);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update LED: %s", esp_err_to_name(err));
        return;
    }

    err = esp_timer_start_once(led->timer,
                               (uint64_t)(led->on ? led->ontime : led->offtime) * 1000);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Failed to restart LED timer: %s", esp_err_to_name(err));
}

static esp_err_t led_set_state(struct led_data *led, uint16_t ontime, uint16_t offtime)
{
    esp_err_t err = esp_timer_stop(led->timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    led->ontime = ontime;
    led->offtime = offtime;
    led->on = ontime != 0;

    err = led_write(led);
    if (err != ESP_OK || ontime == 0 || offtime == 0)
        return err;

    return esp_timer_start_once(led->timer, (uint64_t)ontime * 1000);
}

static void state_timer_callback(void *arg)
{
    struct led_data  *led = arg;
    supla_dev_state_t st;
    esp_err_t         err;

    if (led->manual)
        return;

    if (supla_dev_get_state(led->supla_dev, &st) != SUPLA_RESULT_TRUE)
        return;

    switch (st) {
    case SUPLA_DEV_STATE_CONFIG:
        err = led_set_state(led, 100, 100);
        break;
    case SUPLA_DEV_STATE_IDLE:
        err = led_set_state(led, 1000, 0);
        break;
    case SUPLA_DEV_STATE_INIT:
        err = led_set_state(led, 500, 500);
        break;
    case SUPLA_DEV_STATE_CONNECTED:
        err = led_set_state(led, 200, 200);
        break;
    case SUPLA_DEV_STATE_REGISTERED:
        err = led_set_state(led, 10, 10);
        break;
    case SUPLA_DEV_STATE_ONLINE:
        err = led_set_state(led, led->conf.online_set, 0);
        break;
    default:
        return;
    }

    if (err != ESP_OK)
        ESP_LOGE(TAG, "Failed to update status pattern: %s", esp_err_to_name(err));
}

supla_status_led_t supla_status_led_init(supla_dev_t *dev, const struct status_led_config *config)
{
    struct led_data *led;
    esp_err_t err;

    if (!dev || !config)
        return NULL;

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

    err = gpio_config(&gpio_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LED GPIO: %s", esp_err_to_name(err));
        free(led);
        return NULL;
    }

    err = esp_timer_create(&led_timer_args, &led->timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED timer: %s", esp_err_to_name(err));
        free(led);
        return NULL;
    }
    err = esp_timer_create(&state_timer_args, &led->state_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create state timer: %s", esp_err_to_name(err));
        esp_timer_delete(led->timer);
        free(led);
        return NULL;
    }
    err = esp_timer_start_periodic(led->state_timer, 1000 * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start state timer: %s", esp_err_to_name(err));
        esp_timer_delete(led->state_timer);
        esp_timer_delete(led->timer);
        free(led);
        return NULL;
    }
    return led;
}

esp_err_t supla_status_led_set(supla_status_led_t led, uint16_t on_ms, uint16_t off_ms)
{
    struct led_data *data = led;
    if (!data)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = esp_timer_stop(data->state_timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    data->manual = true;
    return led_set_state(data, on_ms, off_ms);
}

esp_err_t supla_status_led_enable_state_indication(supla_status_led_t led)
{
    struct led_data *data = led;
    if (!data)
        return ESP_ERR_INVALID_ARG;
    if (!data->manual)
        return ESP_OK;

    data->manual = false;
    esp_err_t err = esp_timer_start_periodic(data->state_timer, 1000 * 1000);
    if (err != ESP_OK) {
        data->manual = true;
        return err;
    }

    state_timer_callback(data);
    return ESP_OK;
}

int supla_status_led_delete(supla_status_led_t led)
{
    struct led_data *data = led;

    if (!data)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = esp_timer_stop(data->timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_LOGE(TAG, "Failed to stop LED timer: %s", esp_err_to_name(err));
    err = esp_timer_stop(data->state_timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_LOGE(TAG, "Failed to stop state timer: %s", esp_err_to_name(err));
    err = esp_timer_delete(data->timer);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Failed to delete LED timer: %s", esp_err_to_name(err));
    esp_err_t state_err = esp_timer_delete(data->state_timer);
    if (state_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete state timer: %s", esp_err_to_name(state_err));
        if (err == ESP_OK)
            err = state_err;
    }
    free(data);
    return err;
}
