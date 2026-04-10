/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/ledsmart-ble-channel.h"
#include <stdlib.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp-supla.h>

static const char *TAG = "BLE-CH";

struct lamp_ble_nvs_state {
    int active_func;
};

struct lamp_ble_channel_data {
    struct lamp_ble_channel_config config;
    struct lamp_ble_nvs_state      nvs_state;
    lampsmart_ble_t                light;
};

static const char *variant_name(lampsmart_variant_t variant)
{
    switch (variant) {
    case LAMPSMART_VARIANT_3:
        return "V3";
    case LAMPSMART_VARIANT_2:
        return "V2";
    case LAMPSMART_VARIANT_1A:
        return "V1A";
    case LAMPSMART_VARIANT_1B:
        return "V1B";
    default:
        return "UNKNOWN";
    }
}

static int lamp_ble_channel_init(supla_channel_t *ch)
{
    struct lamp_ble_channel_data *data = supla_channel_get_data(ch);
    const int                     ch_num = supla_channel_get_assigned_number(ch);
    esp_err_t                     rc;

    ESP_LOGI(TAG, "ch[%d] init", ch_num);
    rc = supla_esp_nvs_channel_state_restore(ch, &data->nvs_state, sizeof(data->nvs_state));
    if (rc == ESP_OK) {
        ESP_LOGI(TAG, "ch[%d] nvs read OK:func=%d", ch_num, data->nvs_state.active_func);
        supla_channel_set_active_function(ch, data->nvs_state.active_func);
    }
    return SUPLA_RESULTCODE_TRUE;
}

static int lamp_ble_channel_on_cfg_recv(supla_channel_t *ch, TSD_ChannelConfig *config)
{
    struct lamp_ble_channel_data *data = supla_channel_get_data(ch);
    ESP_LOGI(TAG, "ch[%d] got config fn=%d", config->ChannelNumber, config->Func);

    switch (config->Func) {
    case SUPLA_CHANNELFNC_DIMMER:
    case SUPLA_CHANNELFNC_DIMMER_CCT:
        data->nvs_state.active_func = config->Func;
        supla_esp_nvs_channel_state_store(ch, &data->nvs_state, sizeof(data->nvs_state));
        break;
    default:
        break;
    }
    return ESP_OK;
}

int lamp_ble_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    struct lamp_ble_channel_data *data = supla_channel_get_data(ch);

    const int ch_num = supla_channel_get_assigned_number(ch);
    const int active_func = data->nvs_state.active_func;

    TRGBW_Value *rgbw = (TRGBW_Value *)new_value->value;

    uint8_t br = rgbw->brightness;
    uint8_t wt = rgbw->whiteTemperature;
    uint8_t cold, warm;

    switch (active_func) {
    case SUPLA_CHANNELFNC_DIMMER: {
        uint8_t val = (uint16_t)br * 255 / 100;
        if (rgbw->brightness == 0)
            lampsmart_ble_turn_off(data->light);
        else
            lampsmart_ble_set_levels(data->light, val, val);
    } break;
    case SUPLA_CHANNELFNC_DIMMER_CCT: {
        /* Normalize so the dominant channel reaches 255 at full brightness,
         * then scale both channels by brightness */
        uint8_t max_wt = (wt >= 50) ? wt : (100 - wt);
        cold = (uint32_t)wt * br * 255U / ((uint32_t)max_wt * 100U);
        warm = (uint32_t)(100 - wt) * br * 255U / ((uint32_t)max_wt * 100U);

        if (rgbw->brightness == 0)
            lampsmart_ble_turn_off(data->light);
        else
            lampsmart_ble_set_levels(data->light, cold, warm);

        ESP_LOGI(TAG, "ch[%d] val: BR=%d WT=%d cold=%d warm=%d", ch_num, rgbw->brightness,
                 rgbw->whiteTemperature, cold, warm);
    } break;
    default:
        break;
    }
    return supla_channel_set_rgbw_value(ch, rgbw);
}

supla_channel_t *lamp_ble_channel_create(const struct lamp_ble_channel_config *conf)
{
    struct lamp_ble_channel_data *data;

    const supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_DIMMER,
        .supported_functions = SUPLA_RGBW_BIT_FUNC_DIMMER | SUPLA_RGBW_BIT_FUNC_DIMMER_CCT,
        .default_function = SUPLA_CHANNELFNC_DIMMER_CCT,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
        .on_channel_init = lamp_ble_channel_init,
        .on_set_value = lamp_ble_channel_set_value,
        .on_config_recv = lamp_ble_channel_on_cfg_recv
    };

    supla_channel_t *ch = supla_channel_create(&supla_channel_config);
    if (!ch)
        return NULL;

    data = malloc(sizeof(struct lamp_ble_channel_data));
    if (!data) {
        supla_channel_free(ch);
        return NULL;
    }

    data->config = *conf;
    supla_channel_set_data(ch, data);

    if (lampsmart_ble_init(&data->light, &data->config.lamp_config) != ESP_OK) {
        supla_channel_free(ch);
        free(data);
        return NULL;
    }

    ESP_LOGI(TAG,
             "LampSmart controller initialized for group: 0x%08" PRIX32
             " variant=%s duration_ms=%" PRIu32,
             data->config.lamp_config.group_id, variant_name(data->config.lamp_config.variant),
             data->config.lamp_config.tx_duration_ms);
    return ch;
}

int lamp_ble_channel_set_protocol(supla_channel_t *ch, lampsmart_variant_t variant)
{
    struct lamp_ble_channel_data *data = supla_channel_get_data(ch);
    const int                     ch_num = supla_channel_get_assigned_number(ch);

    lampsmart_ble_config_t cfg;
    if (lampsmart_ble_get_config(data->light, &cfg) == ESP_OK) {
        cfg.variant = variant;
        ESP_LOGI(TAG, "ch[%d] set protocol variant %s", ch_num, variant_name(variant));
        return lampsmart_ble_set_config(data->light, &cfg);
    }
    return ESP_ERR_INVALID_STATE;
}

int lamp_ble_channel_set_group_id(supla_channel_t *ch, uint32_t group_id)
{
    struct lamp_ble_channel_data *data = supla_channel_get_data(ch);
    const int                     ch_num = supla_channel_get_assigned_number(ch);
    lampsmart_ble_config_t        cfg;
    if (lampsmart_ble_get_config(data->light, &cfg) == ESP_OK) {
        cfg.group_id = group_id;
        ESP_LOGI(TAG, "ch[%d] set group_id 0x%08" PRIX32, ch_num, group_id);
        return lampsmart_ble_set_config(data->light, &cfg);
    }
    return ESP_ERR_INVALID_STATE;
}

int lamp_ble_channel_pair(supla_channel_t *ch)
{
    struct lamp_ble_channel_data *data = supla_channel_get_data(ch);
    const int                     ch_num = supla_channel_get_assigned_number(ch);

    ESP_LOGI(TAG, "ch[%d] BLE  pairing started using SmartLamp protocol %s", ch_num,
             variant_name(data->config.lamp_config.variant));
    return lampsmart_ble_pair(data->light);
}