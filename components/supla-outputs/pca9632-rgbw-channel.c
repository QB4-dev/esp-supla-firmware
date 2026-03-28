/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/pca9632-rgbw-channel.h"
#include <stdlib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp-supla.h>

#define CHANNEL_MUTEX_TIMEOUT 1000 //ms

#define CHANNEL_SEMAPHORE_TAKE(mutex)                                       \
    do {                                                                    \
        if (!xSemaphoreTake(mutex, pdMS_TO_TICKS(CHANNEL_MUTEX_TIMEOUT))) { \
            ESP_LOGE(TAG, "can't take mutex");                              \
            return ESP_ERR_TIMEOUT;                                         \
        }                                                                   \
    } while (0)

#define CHANNEL_SEMAPHORE_GIVE(mutex)          \
    do {                                       \
        if (!xSemaphoreGive(mutex)) {          \
            ESP_LOGE(TAG, "can't give mutex"); \
            return ESP_FAIL;                   \
        }                                      \
    } while (0)

static const char *TAG = "PCA9632-CH";

struct rgbw_nvs_state {
    int active_func;
};

struct channel_data {
    SemaphoreHandle_t  mutex;
    i2c_dev_t         *i2c_dev;
    TRGBW_Value        rgbw_value;
    TRGBW_Value        rgbw_target;
    esp_timer_handle_t timer;
    union {
        struct {
            rgbw_mapping_t rgbw_map; //for RGBW channel
            cct_mapping_t  cct_map;
        };
        pca9632_led_t output; //for dimmer channel
    };
    struct rgbw_nvs_state nvs_state;
};

static int pca9632_channel_init(supla_channel_t *ch)
{
    struct channel_data *data = supla_channel_get_data(ch);
    const int            ch_num = supla_channel_get_assigned_number(ch);
    esp_err_t            rc;

    ESP_LOGI(TAG, "ch[%d] init", ch_num);
    rc = supla_esp_nvs_channel_state_restore(ch, &data->nvs_state, sizeof(data->nvs_state));
    if (rc == ESP_OK) {
        ESP_LOGI(TAG, "ch[%d] nvs read OK:func=%d", ch_num, data->nvs_state.active_func);
        supla_channel_set_active_function(ch, data->nvs_state.active_func);
    }
    return SUPLA_RESULTCODE_TRUE;
}

static int pca9632_channel_on_cfg_recv(supla_channel_t *ch, TSD_ChannelConfig *config)
{
    struct channel_data *data = supla_channel_get_data(ch);
    ESP_LOGI(TAG, "ch[%d] got config fn=%d", config->ChannelNumber, config->Func);

    switch (config->Func) {
    case SUPLA_CHANNELFNC_DIMMER:
    case SUPLA_CHANNELFNC_RGBLIGHTING:
    case SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING:
    case SUPLA_CHANNELFNC_DIMMER_CCT:
    case SUPLA_CHANNELFNC_DIMMER_CCT_AND_RGB:
        data->nvs_state.active_func = config->Func;
        break;
    default:
        break;
    }
    return ESP_OK;
}

static uint8_t value_fade_tick(uint8_t val, uint8_t target, uint8_t step)
{
    if (val < target && (val + step) <= target)
        return val + step;
    else if (val > target && (val - step) >= target)
        return val - step;
    else
        return target;
}

int pca9632_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    TRGBW_Value         *rgbw = (TRGBW_Value *)new_value->value;
    struct channel_data *ch_data;
    const int            ch_num = supla_channel_get_assigned_number(ch);

    ESP_LOGI(TAG, "ch[%d] val: BR=%d CB=%d R=%d G=%d B=%d WT=%d", ch_num, rgbw->brightness,
             rgbw->colorBrightness, rgbw->R, rgbw->G, rgbw->B, rgbw->whiteTemperature);

    if (!ch)
        return ESP_ERR_INVALID_ARG;

    ch_data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(ch_data->mutex);
    ch_data->rgbw_target = *rgbw;
    CHANNEL_SEMAPHORE_GIVE(ch_data->mutex);

    return supla_channel_set_rgbw_value(ch, rgbw);
}

static esp_err_t pca9632_set_rgbw_value(struct channel_data *ch_data)
{
    i2c_dev_t     *pca9632;
    int            active_func;
    rgbw_mapping_t rgbw_map;
    cct_mapping_t  cct_map;
    uint8_t        r, g, b, w, br, cb, wt;
    uint8_t        warm, cold;

    CHANNEL_SEMAPHORE_TAKE(ch_data->mutex);
    active_func = ch_data->nvs_state.active_func;
    rgbw_map = ch_data->rgbw_map;
    cct_map = ch_data->cct_map;

    ch_data->rgbw_value.brightness =
        value_fade_tick(ch_data->rgbw_value.brightness, ch_data->rgbw_target.brightness, 1);

    ch_data->rgbw_value.colorBrightness = value_fade_tick(ch_data->rgbw_value.colorBrightness,
                                                          ch_data->rgbw_target.colorBrightness, 1);

    ch_data->rgbw_value.R = value_fade_tick(ch_data->rgbw_value.R, ch_data->rgbw_target.R, 5);
    ch_data->rgbw_value.G = value_fade_tick(ch_data->rgbw_value.G, ch_data->rgbw_target.G, 5);
    ch_data->rgbw_value.B = value_fade_tick(ch_data->rgbw_value.B, ch_data->rgbw_target.B, 5);

    ch_data->rgbw_value.whiteTemperature = value_fade_tick(
        ch_data->rgbw_value.whiteTemperature, ch_data->rgbw_target.whiteTemperature, 5);

    pca9632 = ch_data->i2c_dev;
    br = ch_data->rgbw_value.brightness;
    cb = ch_data->rgbw_value.colorBrightness;
    r = (uint16_t)(ch_data->rgbw_value.R * cb / 100);
    g = (uint16_t)(ch_data->rgbw_value.G * cb / 100);
    b = (uint16_t)(ch_data->rgbw_value.B * cb / 100);
    w = (uint16_t)(ch_data->rgbw_value.brightness * 0xFF / 100);
    wt = ch_data->rgbw_value.whiteTemperature;
    CHANNEL_SEMAPHORE_GIVE(ch_data->mutex);

    switch (active_func) {
    case SUPLA_CHANNELFNC_DIMMER:
        return pca9632_set_pwm_all(pca9632, w, w, w, w);
        break;
    case SUPLA_CHANNELFNC_RGBLIGHTING:
    case SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING:
        w = (active_func == SUPLA_CHANNELFNC_RGBLIGHTING) ? 0 : w;
        switch (rgbw_map) {
        case RGBW_MAP_RGBW:
            return pca9632_set_pwm_all(pca9632, r, g, b, w);
        case RGBW_MAP_GBRW:
            return pca9632_set_pwm_all(pca9632, g, b, r, w);
        case RGBW_MAP_GRBW:
            return pca9632_set_pwm_all(pca9632, g, r, b, w);
        case RGBW_MAP_GRWB:
            return pca9632_set_pwm_all(pca9632, g, r, w, b);
        case RGBW_MAP_BRGW:
            return pca9632_set_pwm_all(pca9632, b, r, g, w);
        default:
            break;
        }
        break;
    case SUPLA_CHANNELFNC_DIMMER_CCT:
        /* cold/warm balance */
        cold = (uint16_t)(wt * 0xFF / 100);
        warm = (uint16_t)((100 - wt) * 0xFF / 100);
        /* scale by brightness */
        cold = (uint16_t)(cold * br / 100);
        warm = (uint16_t)(warm * br / 100);
        switch (cct_map) {
        case CCT_MAP_WWCC:
            return pca9632_set_pwm_all(pca9632, warm, warm, cold, cold);
        case CCT_MAP_WCWC:
            return pca9632_set_pwm_all(pca9632, warm, cold, warm, cold);
        case CCT_MAP_CWCW:
            return pca9632_set_pwm_all(pca9632, cold, warm, cold, warm);
        case CCT_MAP_CCWW:
            return pca9632_set_pwm_all(pca9632, cold, cold, warm, warm);
        default:
            break;
        }
        break;
    default:
        break;
    }
    return ESP_ERR_INVALID_STATE;
}

static void rgb_deferred_fade(void *ch)
{
    struct channel_data *ch_data = supla_channel_get_data(ch);
    pca9632_set_rgbw_value(ch_data);
}

supla_channel_t *pca9632_rgbw_channel_create(const struct pca9632_rgbw_channel_config *config)
{
    supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_DIMMER,
        .supported_functions = SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_RGB_LIGHTING |
                               SUPLA_RGBW_BIT_FUNC_DIMMER_AND_RGB_LIGHTING |
                               SUPLA_RGBW_BIT_FUNC_DIMMER_CCT,
        .default_function = SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE | SUPLA_CHANNEL_FLAG_RGBW_COMMANDS_SUPPORTED,
        .on_channel_init = pca9632_channel_init,
        .on_set_value = pca9632_channel_set_value,
        .on_config_recv = pca9632_channel_on_cfg_recv
    };
    esp_timer_create_args_t timer_args = {
        .name = "rgb-fade",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = rgb_deferred_fade,
    };
    struct channel_data *ch_data;

    supla_channel_t *ch = supla_channel_create(&supla_channel_config);
    if (!ch)
        return NULL;

    ch_data = calloc(1, sizeof(struct channel_data));
    if (!ch_data) {
        supla_channel_free(ch);
        return NULL;
    }

    ch_data->mutex = xSemaphoreCreateMutex();
    ch_data->i2c_dev = config->pca9632;
    ch_data->rgbw_map = config->rgbw_map;
    ch_data->cct_map = config->cct_map;
    timer_args.arg = ch;

    supla_channel_set_data(ch, ch_data);
    pca9632_set_pwm_all(ch_data->i2c_dev, 0x00, 0x00, 0x00, 0x00);
    esp_timer_create(&timer_args, &ch_data->timer);
    esp_timer_start_periodic(ch_data->timer, 10000);
    return ch;
}

static esp_err_t pca9632_set_dimmer_value(struct channel_data *ch_data)
{
    i2c_dev_t    *pca9632;
    pca9632_led_t out;
    uint8_t       brightness;

    CHANNEL_SEMAPHORE_TAKE(ch_data->mutex);
    pca9632 = ch_data->i2c_dev;
    ch_data->rgbw_value.brightness =
        value_fade_tick(ch_data->rgbw_value.brightness, ch_data->rgbw_target.brightness, 1);

    out = ch_data->output;
    brightness = ch_data->rgbw_value.brightness;
    CHANNEL_SEMAPHORE_GIVE(ch_data->mutex);

    return pca9632_set_pwm(pca9632, out, brightness);
}

static void dimmer_deferred_fade(void *ch)
{
    struct channel_data *ch_data = supla_channel_get_data(ch);
    pca9632_set_dimmer_value(ch_data);
}

supla_channel_t *pca9632_dimmer_channel_create(const struct pca9632_dimmer_channel_config *config)
{
    const char *const out_captions[] = {
        [LED0] = "LED0", [LED1] = "LED1", [LED2] = "LED2", [LED3] = "LED3"
    };

    supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_DIMMER,
        .supported_functions = 0xFFFF,
        .default_function = SUPLA_CHANNELFNC_DIMMER,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
        .on_set_value = pca9632_channel_set_value,
        .default_caption = out_captions[config->pwm_output] //
    };

    esp_timer_create_args_t timer_args = {
        .name = "dimmer-fade",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = dimmer_deferred_fade,
    };
    struct channel_data *ch_data;

    supla_channel_t *ch = supla_channel_create(&supla_channel_config);
    if (!ch)
        return NULL;

    ch_data = calloc(1, sizeof(struct channel_data));
    if (!ch_data) {
        supla_channel_free(ch);
        return NULL;
    }

    supla_channel_set_data(ch, ch_data);
    ch_data->mutex = xSemaphoreCreateMutex();
    ch_data->i2c_dev = config->pca9632;
    ch_data->output = config->pwm_output;
    timer_args.arg = ch;

    pca9632_set_pwm(ch_data->i2c_dev, ch_data->output, 0x00);
    esp_timer_create(&timer_args, &ch_data->timer);
    esp_timer_start_periodic(ch_data->timer, 10000);
    return ch;
}
