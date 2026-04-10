/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/rgbw-channel.h"
#include <stdlib.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp-supla.h>

static const char *TAG = "RGBW-CH";

struct rgbw_nvs_state {
    int active_func;
};

struct rgbw_channel_data {
    struct rgbw_channel_config config;
    struct rgbw_nvs_state      nvs_state;
    uint32_t                   duty_res;

    ledc_channel_config_t ledc_r;
    ledc_channel_config_t ledc_g;
    ledc_channel_config_t ledc_b;
    ledc_channel_config_t ledc_ww;
    ledc_channel_config_t ledc_cw;
};

static int rgbw_channel_init(supla_channel_t *ch)
{
    struct rgbw_channel_data *data = supla_channel_get_data(ch);
    const int                 ch_num = supla_channel_get_assigned_number(ch);
    esp_err_t                 rc;

    ESP_LOGI(TAG, "ch[%d] init", ch_num);
    rc = supla_esp_nvs_channel_state_restore(ch, &data->nvs_state, sizeof(data->nvs_state));
    if (rc == ESP_OK) {
        ESP_LOGI(TAG, "ch[%d] nvs read OK:func=%d", ch_num, data->nvs_state.active_func);
        supla_channel_set_active_function(ch, data->nvs_state.active_func);
    }
    return SUPLA_RESULTCODE_TRUE;
}

static int rgbw_channel_on_cfg_recv(supla_channel_t *ch, TSD_ChannelConfig *config)
{
    struct rgbw_channel_data *data = supla_channel_get_data(ch);
    ESP_LOGI(TAG, "ch[%d] got config fn=%d", config->ChannelNumber, config->Func);

    switch (config->Func) {
    case SUPLA_CHANNELFNC_DIMMER:
    case SUPLA_CHANNELFNC_RGBLIGHTING:
    case SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING:
    case SUPLA_CHANNELFNC_DIMMER_CCT:
    case SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB:
        data->nvs_state.active_func = config->Func;
        supla_esp_nvs_channel_state_store(ch, &data->nvs_state, sizeof(data->nvs_state));
        break;
    default:
        break;
    }
    return ESP_OK;
}

int rgbw_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    struct rgbw_channel_data   *data = supla_channel_get_data(ch);
    struct rgbw_channel_config *conf = &data->config;
    const int                   active_func = data->nvs_state.active_func;
    TRGBW_Value                *rgbw = (TRGBW_Value *)new_value->value;

    uint32_t r, g, b, w, cb, wt;
    uint32_t cold, warm;
    uint32_t f = conf->fade_time;

    supla_log(LOG_INFO, "new RGBW val: R=%d G=%d B=%d CB=%d W=%d", rgbw->R, rgbw->G, rgbw->B,
              rgbw->colorBrightness, rgbw->brightness);

    cb = rgbw->colorBrightness;
    r = ((rgbw->R * cb) / 100 * data->duty_res) / 100;
    g = ((rgbw->G * cb) / 100 * data->duty_res) / 100;
    b = ((rgbw->B * cb) / 100 * data->duty_res) / 100;
    w = (rgbw->brightness * data->duty_res) / 100;
    wt = rgbw->whiteTemperature;

    cold = (wt * w) / 100;
    warm = ((100 - wt) * w) / 100;

    switch (active_func) {
    case SUPLA_CHANNELFNC_DIMMER: {
        ledc_set_fade_with_time(data->ledc_r.speed_mode, data->ledc_r.channel, w, f);
        ledc_set_fade_with_time(data->ledc_g.speed_mode, data->ledc_g.channel, w, f);
        ledc_set_fade_with_time(data->ledc_b.speed_mode, data->ledc_b.channel, w, f);

        ledc_fade_start(data->ledc_r.speed_mode, data->ledc_r.channel, LEDC_FADE_NO_WAIT);
        ledc_fade_start(data->ledc_g.speed_mode, data->ledc_g.channel, LEDC_FADE_NO_WAIT);
        ledc_fade_start(data->ledc_b.speed_mode, data->ledc_b.channel, LEDC_FADE_NO_WAIT);

        if (conf->gpio_ww != GPIO_NUM_NC) {
            ledc_set_fade_with_time(data->ledc_ww.speed_mode, data->ledc_ww.channel, w, f);
            ledc_fade_start(data->ledc_ww.speed_mode, data->ledc_ww.channel, LEDC_FADE_NO_WAIT);
        }

        if (conf->gpio_cw != GPIO_NUM_NC) {
            ledc_set_fade_with_time(data->ledc_cw.speed_mode, data->ledc_cw.channel, w, f);
            ledc_fade_start(data->ledc_cw.speed_mode, data->ledc_cw.channel, LEDC_FADE_NO_WAIT);
        }
    } break;
    case SUPLA_CHANNELFNC_RGBLIGHTING:
    case SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING:
    case SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB: {
        ledc_set_fade_with_time(data->ledc_r.speed_mode, data->ledc_r.channel, r, f);
        ledc_set_fade_with_time(data->ledc_g.speed_mode, data->ledc_g.channel, g, f);
        ledc_set_fade_with_time(data->ledc_b.speed_mode, data->ledc_b.channel, b, f);

        ledc_fade_start(data->ledc_r.speed_mode, data->ledc_r.channel, LEDC_FADE_NO_WAIT);
        ledc_fade_start(data->ledc_g.speed_mode, data->ledc_g.channel, LEDC_FADE_NO_WAIT);
        ledc_fade_start(data->ledc_b.speed_mode, data->ledc_b.channel, LEDC_FADE_NO_WAIT);

        if (active_func == SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING) {
            if (conf->gpio_ww != GPIO_NUM_NC) {
                ledc_set_fade_with_time(data->ledc_ww.speed_mode, data->ledc_ww.channel, w, f);
                ledc_fade_start(data->ledc_ww.speed_mode, data->ledc_ww.channel, LEDC_FADE_NO_WAIT);
            }

            if (conf->gpio_cw != GPIO_NUM_NC) {
                ledc_set_fade_with_time(data->ledc_cw.speed_mode, data->ledc_cw.channel, w, f);
                ledc_fade_start(data->ledc_cw.speed_mode, data->ledc_cw.channel, LEDC_FADE_NO_WAIT);
            }
        } else if (active_func == SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB) {
            if (conf->gpio_ww != GPIO_NUM_NC) {
                ledc_set_fade_with_time(data->ledc_ww.speed_mode, data->ledc_ww.channel, warm, f);
                ledc_fade_start(data->ledc_ww.speed_mode, data->ledc_ww.channel, LEDC_FADE_NO_WAIT);
            }

            if (conf->gpio_cw != GPIO_NUM_NC) {
                ledc_set_fade_with_time(data->ledc_cw.speed_mode, data->ledc_cw.channel, cold, f);
                ledc_fade_start(data->ledc_cw.speed_mode, data->ledc_cw.channel, LEDC_FADE_NO_WAIT);
            }
        }
    } break;
    case SUPLA_CHANNELFNC_DIMMER_CCT: {
        if (conf->gpio_ww != GPIO_NUM_NC) {
            ledc_set_fade_with_time(data->ledc_ww.speed_mode, data->ledc_ww.channel, warm, f);
            ledc_fade_start(data->ledc_ww.speed_mode, data->ledc_ww.channel, LEDC_FADE_NO_WAIT);
        }

        if (conf->gpio_cw != GPIO_NUM_NC) {
            ledc_set_fade_with_time(data->ledc_cw.speed_mode, data->ledc_cw.channel, cold, f);
            ledc_fade_start(data->ledc_cw.speed_mode, data->ledc_cw.channel, LEDC_FADE_NO_WAIT);
        }
    } break;
    default:
        break;
    }

    return supla_channel_set_rgbw_value(ch, rgbw);
}

supla_channel_t *rgbw_channel_create(const struct rgbw_channel_config *conf)
{
    supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_DIMMER,
        .supported_functions = SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
                               SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
                               SUPLA_RGBW_BIT_FUNC_DIMMER_CCT |
                               SUPLA_RGBW_BIT_FUNC_DIMMER_CCT_AND_RGB,
        .default_function = SUPLA_CHANNELFNC_RGBLIGHTING,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE, //SUPLA_CHANNEL_FLAG_RGBW_COMMANDS_SUPPORTED
        .on_channel_init = rgbw_channel_init,
        .on_set_value = rgbw_channel_set_value,
        .on_config_recv = rgbw_channel_on_cfg_recv
    };

    ledc_timer_config_t ledc_timer_conf = {
        .timer_num = LEDC_TIMER_0,            // timer index
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .speed_mode = LEDC_LOW_SPEED_MODE,    // timer mode
        .freq_hz = 200,                       // frequency of PWM signal
    };

    struct rgbw_channel_data *data;

    supla_channel_t *ch = supla_channel_create(&supla_channel_config);
    if (!ch)
        return NULL;

    data = malloc(sizeof(struct rgbw_channel_data));
    if (!data) {
        supla_channel_free(ch);
        return NULL;
    }

    data->config = *conf;
    // Configure LEDC channels for RGBW
    data->ledc_r.channel = LEDC_CHANNEL_0;
    data->ledc_r.gpio_num = conf->gpio_r;
    data->ledc_r.speed_mode = LEDC_LOW_SPEED_MODE;
    data->ledc_r.hpoint = 0;
    data->ledc_r.timer_sel = LEDC_TIMER_0;
    data->ledc_r.duty = 0;

    data->ledc_g.channel = LEDC_CHANNEL_1;
    data->ledc_g.gpio_num = conf->gpio_g;
    data->ledc_g.speed_mode = LEDC_LOW_SPEED_MODE;
    data->ledc_g.hpoint = 0;
    data->ledc_g.timer_sel = LEDC_TIMER_0;
    data->ledc_g.duty = 0;

    data->ledc_b.channel = LEDC_CHANNEL_2;
    data->ledc_b.gpio_num = conf->gpio_b;
    data->ledc_b.speed_mode = LEDC_LOW_SPEED_MODE;
    data->ledc_b.hpoint = 0;
    data->ledc_b.timer_sel = LEDC_TIMER_0;
    data->ledc_b.duty = 0;

    if (conf->gpio_ww != GPIO_NUM_NC) {
        data->ledc_ww.channel = LEDC_CHANNEL_3;
        data->ledc_ww.gpio_num = conf->gpio_ww;
        data->ledc_ww.speed_mode = LEDC_LOW_SPEED_MODE;
        data->ledc_ww.hpoint = 0;
        data->ledc_ww.timer_sel = LEDC_TIMER_0;
        data->ledc_ww.duty = 0;
    }

    if (conf->gpio_cw != GPIO_NUM_NC) {
        data->ledc_cw.channel = LEDC_CHANNEL_4;
        data->ledc_cw.gpio_num = conf->gpio_cw;
        data->ledc_cw.speed_mode = LEDC_LOW_SPEED_MODE;
        data->ledc_cw.hpoint = 0;
        data->ledc_cw.timer_sel = LEDC_TIMER_0;
        data->ledc_cw.duty = 0;
    }

    //fade_time can't be less than 10
    data->config.fade_time = conf->fade_time ? conf->fade_time : 10;
    data->duty_res = (1 << ledc_timer_conf.duty_resolution) - 1;

    supla_channel_set_data(ch, data);
    ledc_timer_config(&ledc_timer_conf);

    ledc_channel_config(&data->ledc_r);
    ledc_channel_config(&data->ledc_g);
    ledc_channel_config(&data->ledc_b);

    if (conf->gpio_ww != GPIO_NUM_NC)
        ledc_channel_config(&data->ledc_ww);

    if (conf->gpio_cw != GPIO_NUM_NC)
        ledc_channel_config(&data->ledc_cw);

    ledc_fade_func_install(0);
    return ch;
}
