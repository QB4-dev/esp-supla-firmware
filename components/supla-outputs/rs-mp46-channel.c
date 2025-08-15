/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/rs-mp46-channel.h"

#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_timer.h>

#define DP_MP46_MANUAL_CTRL 1
#define DP_MP46_PERCENT_CTRL 2
#define DP_MP46_CURRENT_POSITION 3
#define DP_MP46_MOTOR_DIRECTION 5
#define DP_MP46_MOVEMENT_INFO 7
#define DP_MP46_PULL_TO_START 102

#define CHANNEL_SEMAPHORE_TAKE(mutex)                      \
    do {                                                   \
        if (!xSemaphoreTake(mutex, pdMS_TO_TICKS(1000))) { \
            ESP_LOGE(TAG, "can't take mutex");             \
            return ESP_ERR_TIMEOUT;                        \
        }                                                  \
    } while (0)

#define CHANNEL_SEMAPHORE_GIVE(mutex)          \
    do {                                       \
        if (!xSemaphoreGive(mutex)) {          \
            ESP_LOGE(TAG, "can't give mutex"); \
            return ESP_FAIL;                   \
        }                                      \
    } while (0)

static const char *TAG = "RS-MP46";

struct mp46_rs_channel_data {
    esp_tuya_mcu_handle_t tuya_mcu;
};

static void mcu_event_handler(void *event_handler_arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    supla_channel_t *ch = event_handler_arg;
    supla_dev_t     *dev = supla_channel_get_assigned_device(ch);

    switch (event_id) {
    case TUYA_MCU_EVENT_STATE_CHANGED:
        ESP_LOGI(TAG, "TUYA MCU state changed");
        break;
    case TUYA_MCU_EVENT_CONFIG_REQUEST:
        ESP_LOGI(TAG, "TUYA MCU config request");
        supla_dev_enter_config_mode(dev);
        break;
    case TUYA_MCU_EVENT_DP_UPDATE: {
        tuya_dp_t *dp = (tuya_dp_t *)event_data;
        ESP_LOGI(TAG, "TUYA MCU data point update");
        tuya_dp_print(event_data);
        switch (dp->id) {
        case DP_MP46_CURRENT_POSITION: {
            TDSC_RollerShutterValue rs_val = {};
            rs_val.position = dp->data.value;
            supla_channel_set_roller_shutter_value(ch, &rs_val);
        } break;
        case DP_MP46_MOTOR_DIRECTION: {
        } break;

        case DP_MP46_PULL_TO_START: {
        } break;
        default:
            break;
        }
    } break;
    default:
        break;
    }
}

static int rs_channel_set(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    char task = new_value->value[0];
    switch (task) {
    case 0: // STOP
        supla_mp46_rs_channel_manual_ctrl(ch, MP46_RS_MANUAL_STOP);
        break;
    case 1: //MOVE DOWN
        supla_mp46_rs_channel_manual_ctrl(ch, MP46_RS_MANUAL_OPEN);
        break;
    case 2: //MOVE UP
        supla_mp46_rs_channel_manual_ctrl(ch, MP46_RS_MANUAL_CLOSE);
        break;
    case 3: //DOWN_OR_STOP
        //supla_rs_channel_move_down_or_stop(ch);
        break;
    case 4: //UP_OR_STOP
        //supla_rs_channel_move_up_or_stop(ch);
        break;
    case 5: // STEP_BY_STEP
        //supla_rs_channel_step_by_step(ch);
        break;
    default:
        if (task >= 10 && task <= 110)
            supla_mp46_rs_channel_set_target_position(ch, task - 10);
        break;
    }
    return SUPLA_RESULT_TRUE;
}

supla_channel_t *supla_mp46_rs_channel_create(const struct mp46_rs_channel_config *config)
{
    const supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_RELAY,
        .default_function = SUPLA_CHANNELFNC_CURTAIN, //
        .supported_functions = SUPLA_BIT_FUNC_CURTAIN,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
        .on_set_value = rs_channel_set,
    };
    struct mp46_rs_channel_data *data;

    supla_channel_t *ch = supla_channel_create(&supla_channel_config);
    if (!ch)
        return NULL;

    data = calloc(1, sizeof(struct mp46_rs_channel_data));
    if (!data) {
        supla_channel_free(ch);
        return NULL;
    }

    supla_channel_set_data(ch, data);
    data->tuya_mcu = esp_tuya_mcu_init(&config->uart);
    esp_tuya_mcu_add_handler(data->tuya_mcu, mcu_event_handler, ch);
    return ch;
}

int supla_mp46_rs_channel_delete(supla_channel_t *ch)
{
    struct mp46_rs_channel_data *data = supla_channel_get_data(ch);
    esp_tuya_mcu_deinit(data->tuya_mcu);
    free(data);
    return supla_channel_free(ch);
}

int supla_mp46_rs_channel_manual_ctrl(supla_channel_t *ch, mp46_rs_manual_cmd_t cmd)
{
    struct mp46_rs_channel_data *data = supla_channel_get_data(ch);
    tuya_dp_t                    dp;

    tuya_dp_set_enum(&dp, DP_MP46_MANUAL_CTRL, cmd);
    return esp_tuya_mcu_push_dp(data->tuya_mcu, &dp);
}

int supla_mp46_rs_channel_set_target_position(supla_channel_t *ch, int8_t target)
{
    struct mp46_rs_channel_data *data = supla_channel_get_data(ch);
    tuya_dp_t                    dp;

    tuya_dp_set_value(&dp, DP_MP46_PERCENT_CTRL, target);
    return esp_tuya_mcu_push_dp(data->tuya_mcu, &dp);
}

int supla_mp46_rs_channel_set_direction(supla_channel_t *ch, bool inverted)
{
    struct mp46_rs_channel_data *data = supla_channel_get_data(ch);
    tuya_dp_t                    dp;

    tuya_dp_set_bool(&dp, DP_MP46_MOTOR_DIRECTION, inverted);
    return esp_tuya_mcu_push_dp(data->tuya_mcu, &dp);
}

int supla_mp46_rs_channel_set_pull_to_start(supla_channel_t *ch, bool enabled)
{
    struct mp46_rs_channel_data *data = supla_channel_get_data(ch);
    tuya_dp_t                    dp;

    tuya_dp_set_bool(&dp, DP_MP46_PULL_TO_START, enabled);
    return esp_tuya_mcu_push_dp(data->tuya_mcu, &dp);
}
