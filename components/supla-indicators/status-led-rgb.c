/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <sdkconfig.h>

#if !defined(CONFIG_IDF_TARGET_ESP8266)

#include "include/status-led-rgb.h"
#include <stdlib.h>
#include <esp_timer.h>
#include <esp_log.h>

static const char *TAG = "status-led-rgb";

struct led_rgb_data {
    struct status_led_rgb_config conf;
    supla_dev_t                 *supla_dev;
    led_strip_handle_t           strip;
    rgb_t                        color;
    bool                         on;
    bool                         manual;
    uint16_t                     ontime, offtime;
    esp_timer_handle_t           timer;
    esp_timer_handle_t           state_timer;
};

static esp_err_t led_rgb_write(struct led_rgb_data *led)
{
    rgb_t color = led->on ? led->color : rgb_from_values(0, 0, 0);
    esp_err_t err = led_strip_set_pixel(led->strip, 0, color.r, color.g, color.b);
    if (err != ESP_OK)
        return err;
    return led_strip_refresh(led->strip);
}

static esp_err_t led_rgb_set_state(struct led_rgb_data *led, uint16_t ontime, uint16_t offtime)
{
    esp_err_t err = esp_timer_stop(led->timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    led->ontime = ontime;
    led->offtime = offtime;
    led->on = ontime != 0;

    err = led_rgb_write(led);
    if (err != ESP_OK || ontime == 0 || offtime == 0)
        return err;

    return esp_timer_start_once(led->timer, (uint64_t)ontime * 1000);
}

static void led_rgb_timer_callback(void *arg)
{
    struct led_rgb_data *led = arg;

    led->on = !led->on;
    esp_err_t err = led_rgb_write(led);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update RGB LED: %s", esp_err_to_name(err));
        return;
    }

    err = esp_timer_start_once(led->timer, (uint64_t)(led->on ? led->ontime : led->offtime) * 1000);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Failed to restart RGB LED timer: %s", esp_err_to_name(err));
}

static void led_rgb_state_timer_callback(void *arg)
{
    struct led_rgb_data *led = arg;
    supla_dev_state_t    state;
    esp_err_t            err;

    if (led->manual || supla_dev_get_state(led->supla_dev, &state) != SUPLA_RESULT_TRUE)
        return;

    switch (state) {
    case SUPLA_DEV_STATE_CONFIG:
        err = led_rgb_set_state(led, 100, 100);
        break;
    case SUPLA_DEV_STATE_IDLE:
        err = led_rgb_set_state(led, 1000, 0);
        break;
    case SUPLA_DEV_STATE_INIT:
        err = led_rgb_set_state(led, 500, 500);
        break;
    case SUPLA_DEV_STATE_CONNECTED:
        err = led_rgb_set_state(led, 200, 200);
        break;
    case SUPLA_DEV_STATE_REGISTERED:
        err = led_rgb_set_state(led, 10, 10);
        break;
    case SUPLA_DEV_STATE_ONLINE:
        err = led_rgb_set_state(led, led->conf.online_set, 0);
        break;
    default:
        return;
    }

    if (err != ESP_OK)
        ESP_LOGE(TAG, "Failed to update status pattern: %s", esp_err_to_name(err));
}

static esp_err_t led_rgb_release(struct led_rgb_data *led)
{
    esp_err_t err, result = ESP_OK;

    if (led->state_timer) {
        err = esp_timer_delete(led->state_timer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete state timer: %s", esp_err_to_name(err));
            result = err;
        }
    }
    if (led->timer) {
        err = esp_timer_delete(led->timer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete LED timer: %s", esp_err_to_name(err));
            if (result == ESP_OK)
                result = err;
        }
    }

    if (led->strip) {
        err = led_strip_del(led->strip);
        if (err != ESP_OK && result == ESP_OK)
            result = err;
    }
    free(led);
    return result;
}

supla_status_led_rgb_t supla_status_led_rgb_init(supla_dev_t                        *dev,
                                                 const struct status_led_rgb_config *config)
{
    if (!dev || !config)
        return NULL;

    struct led_rgb_data *led = calloc(1, sizeof(*led));
    if (!led)
        return NULL;

    led->conf = *config;
    led->supla_dev = dev;
    led->color = rgb_is_zero(config->color) ? rgb_from_values(0, 255, 0) : config->color;
    led_strip_config_t strip_config = {
        .strip_gpio_num = config->gpio,
        .max_leds = 1,
        .color_component_format = config->pixel_format,
        .led_model = config->type,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &led->strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RGB LED: %s", esp_err_to_name(err));
        free(led);
        return NULL;
    }

    esp_timer_create_args_t led_timer_args = {
        .name = "status-led-rgb",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = led_rgb_timer_callback,
        .arg = led,
    };
    esp_timer_create_args_t state_timer_args = {
        .name = "supla-state-rgb",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = led_rgb_state_timer_callback,
        .arg = led,
    };

    err = esp_timer_create(&led_timer_args, &led->timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RGB LED timer: %s", esp_err_to_name(err));
        led_rgb_release(led);
        return NULL;
    }
    err = esp_timer_create(&state_timer_args, &led->state_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create state timer: %s", esp_err_to_name(err));
        led_rgb_release(led);
        return NULL;
    }
    err = esp_timer_start_periodic(led->state_timer, 1000 * 1000);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start state timer: %s", esp_err_to_name(err));
        led_rgb_release(led);
        return NULL;
    }
    return led;
}

esp_err_t supla_status_led_rgb_set(supla_status_led_rgb_t led, rgb_t color, uint16_t ontime_ms,
                                   uint16_t offtime_ms)
{
    struct led_rgb_data *data = led;
    if (!data)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = esp_timer_stop(data->state_timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    data->manual = true;
    data->color = color;
    return led_rgb_set_state(data, ontime_ms, offtime_ms);
}

esp_err_t supla_status_led_rgb_enable_state_indication(supla_status_led_rgb_t led)
{
    struct led_rgb_data *data = led;
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

    led_rgb_state_timer_callback(data);
    return ESP_OK;
}

int supla_status_led_rgb_delete(supla_status_led_rgb_t led)
{
    struct led_rgb_data *data = led;
    if (!data)
        return ESP_ERR_INVALID_ARG;

    esp_err_t err = esp_timer_stop(data->state_timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_LOGE(TAG, "Failed to stop state timer: %s", esp_err_to_name(err));
    err = esp_timer_stop(data->timer);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_LOGE(TAG, "Failed to stop LED timer: %s", esp_err_to_name(err));
    return led_rgb_release(data);
}

#endif /* !CONFIG_IDF_TARGET_ESP8266 */
