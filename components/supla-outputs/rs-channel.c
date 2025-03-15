/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/rs-channel.h"
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_timer.h>

#define CHANNEL_SEMAPHORE_TAKE(mutex)                          \
    do {                                                       \
        if (!xSemaphoreTake(mutex, 1000 / portTICK_RATE_MS)) { \
            ESP_LOGE(TAG, "can't take mutex");                 \
            return ESP_ERR_TIMEOUT;                            \
        }                                                      \
    } while (0)

#define CHANNEL_SEMAPHORE_GIVE(mutex)          \
    do {                                       \
        if (!xSemaphoreGive(mutex)) {          \
            ESP_LOGE(TAG, "can't give mutex"); \
            return ESP_FAIL;                   \
        }                                      \
    } while (0)

#define RS_TIMER_INTERVAL 100.0 //ms
#define RS_REPORT_INTERVAL 500  //ms
#define RS_STORE_INTERVAL 10000 //ms

static const char *TAG = "RS-CH";
enum rs_state { RS_STATE_MOVING_UP = -1, RS_STATE_IDLE = 0, RS_STATE_MOVING_DOWN = 1 };

struct rs_nvs_config {
    int    active_func;
    int8_t stored_pos;
    union {
        TChannelConfig_RollerShutter rs;
    };
};

struct rs_channel_data {
    SemaphoreHandle_t    mutex;
    gpio_num_t           gpio_minus;
    gpio_num_t           gpio_plus;
    enum rs_state        state;
    float                real_pos;   // 0 - closed; 100 - opened
    int8_t               target_pos; // 0 - closed; 100 - opened
    TickType_t           report_tick;
    TickType_t           store_tick;
    esp_timer_handle_t   timer;
    struct rs_nvs_config nvs_config;
};

static int supla_rs_channel_init(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    const int               ch_num = supla_channel_get_assigned_number(ch);
    esp_err_t               rc;

    supla_log(LOG_INFO, "ch[%d] rs_ch init", ch_num);
    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    rc = supla_esp_nvs_channel_config_restore(ch, &data->nvs_config, sizeof(data->nvs_config));
    if (rc == ESP_OK) {
        supla_log(LOG_INFO, "ch[%d] rs_ch nvs read OK:func=%d pos=%d", ch_num,
                  data->nvs_config.active_func, data->nvs_config.stored_pos);
        supla_channel_set_active_function(ch, data->nvs_config.active_func);
        data->real_pos = data->nvs_config.stored_pos;
    }
    data->target_pos = data->real_pos;
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return SUPLA_RESULTCODE_TRUE;
}

static int supla_rs_move_up(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    data->state = RS_STATE_MOVING_UP;
    data->target_pos = 0;
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

static int supla_rs_move_down(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    data->state = RS_STATE_MOVING_DOWN;
    data->target_pos = 100;
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

static int supla_rs_stop(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    data->state = RS_STATE_IDLE;
    data->target_pos = data->real_pos;
    gpio_set_level(data->gpio_minus, 0);
    gpio_set_level(data->gpio_plus, 0);
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

static int supla_rs_set_target_position(supla_channel_t *ch, int8_t target)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    data->target_pos = target;
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

static int supla_rs_tick(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    struct rs_nvs_config   *conf = &data->nvs_config;
    const int               ch_num = supla_channel_get_assigned_number(ch);
    float                   pos_diff;
    int8_t                  position;
    TickType_t              ticks = xTaskGetTickCount();
    TDSC_RollerShutterValue rs_val = {};

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    position = data->real_pos;
    if (position == data->target_pos) {
        data->state = RS_STATE_IDLE;
    } else if (position > data->target_pos) {
        data->state = RS_STATE_MOVING_UP;
    } else if (position < data->target_pos) {
        data->state = RS_STATE_MOVING_DOWN;
    }

    switch (data->state) {
    case RS_STATE_IDLE:
    default:
        gpio_set_level(data->gpio_minus, 0);
        gpio_set_level(data->gpio_plus, 0);
        break;
    case RS_STATE_MOVING_UP:
        pos_diff = conf->rs.OpeningTimeMS ? (100 * RS_TIMER_INTERVAL / conf->rs.OpeningTimeMS) : 0;
        data->real_pos = (data->real_pos > 0) ? (data->real_pos - pos_diff) : 0;
        if (pos_diff && data->real_pos > data->target_pos) {
            gpio_set_level(data->gpio_minus, 1);
            gpio_set_level(data->gpio_plus, 0);
        } else {
            data->state = RS_STATE_IDLE;
            data->target_pos = data->real_pos;
            gpio_set_level(data->gpio_minus, 0);
            gpio_set_level(data->gpio_plus, 0);
        }
        break;
    case RS_STATE_MOVING_DOWN:
        pos_diff = conf->rs.ClosingTimeMS ? (100 * RS_TIMER_INTERVAL / conf->rs.ClosingTimeMS) : 0;
        data->real_pos = (data->real_pos < 100) ? (data->real_pos + pos_diff) : 100;
        if (pos_diff && data->real_pos < data->target_pos) {
            gpio_set_level(data->gpio_minus, 0);
            gpio_set_level(data->gpio_plus, 1);
        } else {
            data->state = RS_STATE_IDLE;
            data->target_pos = data->real_pos;
            gpio_set_level(data->gpio_minus, 0);
            gpio_set_level(data->gpio_plus, 0);
        }
        break;
    }

    if (ticks - data->report_tick > pdMS_TO_TICKS(RS_REPORT_INTERVAL)) {
        data->report_tick = ticks;
        rs_val.position = data->real_pos;
        supla_channel_set_roller_shutter_value(ch, &rs_val);
        if (data->state != RS_STATE_IDLE) {
            supla_log(LOG_INFO, "ch[%d] rs tick: %s position=%d target=%d", ch_num,
                      data->state == RS_STATE_IDLE        ? "IDLE" :
                      data->state == RS_STATE_MOVING_UP   ? "MOVING_UP" :
                      data->state == RS_STATE_MOVING_DOWN ? "MOVING_DOWN" :
                                                            "??",
                      rs_val.position, data->target_pos);
        }
    }

    if (ticks - data->store_tick > pdMS_TO_TICKS(RS_STORE_INTERVAL)) {
        data->store_tick = ticks;
        if (data->nvs_config.stored_pos != data->real_pos) {
            data->nvs_config.stored_pos = data->real_pos;
            supla_esp_nvs_channel_config_store(ch, &data->nvs_config, sizeof(data->nvs_config));
        }
    }
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

static void rs_timer_event(void *ch)
{
    supla_rs_tick(ch);
}

static int supla_rs_set(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    char task = new_value->value[0];
    char tilt = new_value->value[1];

    uint32_t open = ((new_value->DurationMS >> 0) & 0xFFFF) * 100;
    uint32_t close = ((new_value->DurationMS >> 16) & 0xFFFF) * 100;

    supla_log(LOG_INFO, "rs set task=%d tilt=%d open=%dms close=%dms", task, tilt, open, close);

    switch (task) {
    case 0: { // STOP
        supla_rs_stop(ch);
        break;
    }
    case 1: { //MOVE DOWN
        supla_rs_move_down(ch);
        break;
    }
    case 2: { //MOVE UP
        supla_rs_move_up(ch);
        break;
    }
    case 3: { // down or stop
              //        if (inMove()) {
              //            stop();
              //        } else {
              //            moveDown();
              //        }
        break;
    }
    case 4: { // up or stop
              //        if (inMove()) {
              //            stop();
              //        } else {
              //            moveUp();
              //        }
        break;
    }
    case 5: { // sbs
              //        if (inMove()) {
              //            stop();
              //        } else if (lastDirectionWasOpen()) {
              //            moveDown();
              //        } else if (lastDirectionWasClose()) {
              //            moveUp();
              //        } else if (currentPosition < 50) {
              //            moveDown();
              //        } else {
              //            moveUp();
              //        }
        break;
    }

    default: {
        if (task >= 10 && task <= 110)
            supla_rs_set_target_position(ch, task - 10);
        break;
    }
    }
    return SUPLA_RESULT_TRUE;
}

static int supla_srv_rs_config(supla_channel_t *ch, TSD_ChannelConfig *config)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    struct rs_nvs_config   *nvs_conf = &data->nvs_config;

    supla_log(LOG_INFO, "ch[%d]  got channel config fn=%d type=%d size=%d", config->ChannelNumber,
              config->Func, config->ConfigType, config->ConfigSize);

    switch (config->Func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
    case SUPLA_CHANNELFNC_CURTAIN:
    case SUPLA_CHANNELFNC_VERTICAL_BLIND:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR: {
        if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
            TChannelConfig_RollerShutter *rs_conf = (TChannelConfig_RollerShutter *)config->Config;
            supla_log(LOG_INFO, "roller shutter config: opening %dms closing %dms",
                      rs_conf->OpeningTimeMS, rs_conf->ClosingTimeMS);

            CHANNEL_SEMAPHORE_TAKE(data->mutex);
            nvs_conf->active_func = config->Func;
            nvs_conf->rs = *rs_conf;
            supla_esp_nvs_channel_config_store(ch, &data->nvs_config, sizeof(data->nvs_config));
            CHANNEL_SEMAPHORE_GIVE(data->mutex);
        }
    } break;
    default:
        break;
    }
    return ESP_OK;
}

supla_channel_t *supla_rs_channel_create(const struct rs_channel_config *config)
{
    const supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_RELAY,
        .supported_functions = config->supported_functions,
        .sync_values_onchange = true,
        .default_function = config->default_function,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
        //		| SUPLA_CHANNEL_FLAG_RS_SBS_AND_STOP_ACTIONS |
        //                 SUPLA_CHANNEL_FLAG_CALCFG_RECALIBRATE |
        //                 SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE,
        .on_channel_init = supla_rs_channel_init,
        .on_set_value = supla_rs_set,
        .on_config_recv = supla_srv_rs_config
    };

    const gpio_config_t gpio_conf = {
        .pin_bit_mask = (1 << config->gpio_fwd) | (1 << config->gpio_bck),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE //
    };

    esp_timer_create_args_t timer_args = {
        .name = "rs",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = rs_timer_event,
    };
    struct rs_channel_data *data;

    supla_channel_t *ch = supla_channel_create(&supla_channel_config);
    if (!ch)
        return NULL;

    data = calloc(1, sizeof(struct rs_channel_data));
    if (!data) {
        supla_channel_free(ch);
        return NULL;
    }

    supla_channel_set_data(ch, data);
    data->mutex = xSemaphoreCreateMutex();
    data->real_pos = 0;
    data->target_pos = data->real_pos;
    data->gpio_minus = config->gpio_fwd;
    data->gpio_plus = config->gpio_bck;

    gpio_config(&gpio_conf);
    gpio_set_level(data->gpio_minus, 0);
    gpio_set_level(data->gpio_plus, 0);
    timer_args.arg = ch;

    esp_timer_create(&timer_args, &data->timer);
    esp_timer_start_periodic(data->timer, RS_TIMER_INTERVAL * 1000);
    return ch;
}
