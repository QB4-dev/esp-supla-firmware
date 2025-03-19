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

#define RS_TIMER_INTERVAL 100   //ms
#define RS_REPORT_INTERVAL 500  //ms
#define RS_STORE_INTERVAL 10000 //ms

static const char *TAG = "RS-CH";
enum rs_state { RS_STATE_OPENING = -1, RS_STATE_IDLE = 0, RS_STATE_CLOSING = 1 };

struct rs_nvs_state {
    int    active_func;
    int8_t stored_pos;
    union {
        TChannelConfig_RollerShutter rs_conf;
        TChannelConfig_FacadeBlind   blinds_conf;
    };
};

struct rs_channel_data {
    SemaphoreHandle_t   mutex;
    gpio_num_t          gpio_open;
    gpio_num_t          gpio_close;
    enum rs_state       state;
    enum rs_state       last_state;
    bool                calibration;
    float               real_pos;   // 0 - closed; 100 - opened
    int8_t              target_pos; // 0 - closed; 100 - opened
    TickType_t          report_tick;
    TickType_t          store_tick;
    esp_timer_handle_t  timer;
    struct rs_nvs_state nvs_state;
};

static int supla_rs_channel_get_base_function(supla_channel_t *ch)
{
    int base_func = SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER;
    int active_func;

    supla_channel_get_active_function(ch, &active_func);
    switch (active_func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW:
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
    case SUPLA_CHANNELFNC_CURTAIN:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR:
        base_func = SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER;
        break;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
    case SUPLA_CHANNELFNC_VERTICAL_BLIND:
        base_func = SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND;
        break;
    default:
        break;
    }
    return base_func;
}

static int supla_rs_channel_get_opening_time(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    struct rs_nvs_state    *conf = &data->nvs_state;
    const int               base_func = supla_rs_channel_get_base_function(ch);

    switch (base_func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
        return conf->rs_conf.OpeningTimeMS;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
        return conf->blinds_conf.OpeningTimeMS;
    default:
        return 0;
    }
}

static int supla_rs_channel_get_closing_time(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    struct rs_nvs_state    *conf = &data->nvs_state;
    const int               base_func = supla_rs_channel_get_base_function(ch);

    switch (base_func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
        return conf->rs_conf.ClosingTimeMS;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
        return conf->blinds_conf.ClosingTimeMS;
    default:
        return 0;
    }
}

static int supla_rs_channel_get_tilting_time(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    struct rs_nvs_state    *conf = &data->nvs_state;
    const int               base_func = supla_rs_channel_get_base_function(ch);

    switch (base_func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
        return 0;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
        return conf->blinds_conf.TiltingTimeMS;
    default:
        return 0;
    }
}

static int supla_rs_channel_init(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    const int               ch_num = supla_channel_get_assigned_number(ch);
    esp_err_t               rc;

    supla_log(LOG_INFO, "ch[%d] rs_ch init", ch_num);
    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    data->real_pos = -1; //need calibration if not read correctly from nvs
    rc = supla_esp_nvs_channel_state_restore(ch, &data->nvs_state, sizeof(data->nvs_state));
    if (rc == ESP_OK) {
        supla_log(LOG_INFO, "ch[%d] rs_ch nvs read OK:func=%d pos=%d", ch_num,
                  data->nvs_state.active_func, data->nvs_state.stored_pos);

        supla_channel_set_active_function(ch, data->nvs_state.active_func);
        data->real_pos = data->nvs_state.stored_pos;
        data->target_pos = data->real_pos;
    }
    //enable calibration if position is unknown
    if (data->real_pos < 0) {
        supla_log(LOG_WARNING, "ch[%d] rs_ch needs calibration", ch_num);
        data->calibration = true;
    }
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return SUPLA_RESULTCODE_TRUE;
}

static int supla_rs_channel_internal_stop(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    if (data->state != RS_STATE_IDLE) {
        data->last_state = data->state;
        switch (data->last_state) {
        case RS_STATE_CLOSING:
            //calibration completed
            if (data->calibration && data->target_pos == 100) {
                data->calibration = false;
                data->real_pos = 100;
            }
            break;
        case RS_STATE_OPENING:
            //calibration completed
            if (data->calibration && data->target_pos == 0) {
                data->calibration = false;
                data->real_pos = 0;
            }
            break;
        default:
            break;
        }
    }

    data->state = RS_STATE_IDLE;
    data->target_pos = data->real_pos;
    gpio_set_level(data->gpio_open, 0);
    gpio_set_level(data->gpio_close, 0);
    return ESP_OK;
}

static int supla_rs_set(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    char task = new_value->value[0];
    char tilt = new_value->value[1];

    uint32_t open = ((new_value->DurationMS >> 0) & 0xFFFF) * 100;
    uint32_t close = ((new_value->DurationMS >> 16) & 0xFFFF) * 100;

    supla_log(LOG_INFO, "rs set task=%d tilt=%d open=%dms close=%dms", task, tilt, open, close);

    switch (task) {
    case 0: // STOP
        supla_rs_channel_move_stop(ch);
        break;
    case 1: //MOVE DOWN
        supla_rs_channel_move_down(ch);
        break;
    case 2: //MOVE UP
        supla_rs_channel_move_up(ch);
        break;
    case 3: //DOWN_OR_STOP
        supla_rs_channel_move_down_or_stop(ch);
        break;
    case 4: //UP_OR_STOP
        supla_rs_channel_move_up_or_stop(ch);
        break;
    case 5: // STEP_BY_STEP
        supla_rs_channel_step_by_step(ch);
        break;
    default:
        if (task >= 10 && task <= 110)
            supla_rs_channel_set_target_position(ch, task - 10);
        break;
    }
    return SUPLA_RESULT_TRUE;
}

static int supla_rs_tick(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    const int               ch_num = supla_channel_get_assigned_number(ch);
    int                     base_func, opening_time, closing_time; // tilting_time;
    float                   diff;
    int8_t                  position;
    TickType_t              ticks = xTaskGetTickCount();

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    base_func = supla_rs_channel_get_base_function(ch);
    opening_time = supla_rs_channel_get_opening_time(ch);
    closing_time = supla_rs_channel_get_closing_time(ch);
    //tilting_time = supla_rs_channel_get_tilting_time(ch); TODO
    position = data->real_pos;

    if (position == data->target_pos) {
        data->state = RS_STATE_IDLE;
    } else if (position > data->target_pos) {
        data->state = RS_STATE_OPENING;
    } else if (position < data->target_pos) {
        data->state = RS_STATE_CLOSING;
    }

    switch (data->state) {
    case RS_STATE_IDLE:
    default:
        gpio_set_level(data->gpio_open, 0);
        gpio_set_level(data->gpio_close, 0);
        break;
    case RS_STATE_OPENING:
        diff = opening_time ? (100.0 * RS_TIMER_INTERVAL / opening_time) : 0;
        data->real_pos = (data->real_pos > 0) ? (data->real_pos - diff) : 0;
        if (diff && data->real_pos > data->target_pos) {
            gpio_set_level(data->gpio_open, 1);
            gpio_set_level(data->gpio_close, 0);
        } else {
            supla_rs_channel_internal_stop(ch);
        }
        break;
    case RS_STATE_CLOSING:
        diff = closing_time ? (100.0 * RS_TIMER_INTERVAL / closing_time) : 0;
        data->real_pos = (data->real_pos < 100) ? (data->real_pos + diff) : 100;
        if (diff && data->real_pos < data->target_pos) {
            gpio_set_level(data->gpio_open, 0);
            gpio_set_level(data->gpio_close, 1);
        } else {
            supla_rs_channel_internal_stop(ch);
        }
        break;
    }

    if (ticks - data->report_tick > pdMS_TO_TICKS(RS_REPORT_INTERVAL)) {
        data->report_tick = ticks;

        if (data->state != RS_STATE_IDLE) {
            supla_log(LOG_INFO, "ch[%d] rs tick: %s position=%d target=%d last state=%d", ch_num,
                      data->state == RS_STATE_IDLE    ? "IDLE" :
                      data->state == RS_STATE_OPENING ? "OPENING" :
                      data->state == RS_STATE_CLOSING ? "CLOSING" :
                                                        "??",
                      position, data->target_pos, data->last_state);
        }

        switch (base_func) {
        case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER: {
            TDSC_RollerShutterValue rs_val = {
                .position = (!data->calibration) ? data->real_pos : -1, // -1: calibration needed
            };
            if (data->calibration && data->state != RS_STATE_IDLE)
                rs_val.flags |= RS_VALUE_FLAG_CALIBRATION_IN_PROGRESS;

            supla_channel_set_roller_shutter_value(ch, &rs_val);
        } break;
        case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND: {
            TDSC_FacadeBlindValue fb_val = {
                .position = (!data->calibration) ? data->real_pos : -1, // -1: calibration needed
                //.flags = RS_VALUE_FLAG_TILT_IS_SET                      //
            };
            if (data->calibration && data->state != RS_STATE_IDLE)
                fb_val.flags |= RS_VALUE_FLAG_CALIBRATION_IN_PROGRESS;

            supla_channel_set_facadeblind_value(ch, &fb_val);
        } break;
        default:
            return 0;
        }
    }

    if (ticks - data->store_tick > pdMS_TO_TICKS(RS_STORE_INTERVAL)) {
        data->store_tick = ticks;
        if (data->state == RS_STATE_IDLE && data->nvs_state.stored_pos != data->real_pos) {
            data->nvs_state.stored_pos = data->real_pos;
            supla_esp_nvs_channel_state_store(ch, &data->nvs_state, sizeof(data->nvs_state));
        }
    }
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

static void rs_timer_event(void *ch)
{
    supla_rs_tick(ch);
}

static int supla_rs_config_to_srv(supla_channel_t *ch, TSDS_SetChannelConfig *config)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    struct rs_nvs_state    *nvs_conf = &data->nvs_state;
    int                     base_func;

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    config->Func = nvs_conf->active_func;

    base_func = supla_rs_channel_get_base_function(ch);
    switch (base_func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER: {
        if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
            TChannelConfig_RollerShutter *rs_conf = &nvs_conf->rs_conf;
            supla_log(LOG_INFO, "ch[%d] send roller shutter config: open=%dms close=%dms",
                      config->ChannelNumber, rs_conf->OpeningTimeMS, rs_conf->ClosingTimeMS);

            config->ConfigSize = sizeof(nvs_conf->rs_conf);
            memcpy(&config->Config, &nvs_conf->rs_conf, sizeof(nvs_conf->rs_conf));
        }
    } break;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND: {
        if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
            TChannelConfig_FacadeBlind *blinds_conf = &nvs_conf->blinds_conf;
            supla_log(LOG_INFO, "ch[%d] send facade blind config: open=%dms close=%dms tilt=%dms",
                      config->ChannelNumber, blinds_conf->OpeningTimeMS, blinds_conf->ClosingTimeMS,
                      blinds_conf->TiltingTimeMS);

            config->ConfigSize = sizeof(nvs_conf->blinds_conf);
            memcpy(&config->Config, &nvs_conf->blinds_conf, sizeof(nvs_conf->blinds_conf));
        }
    } break;
    default:
        break;
    }
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

static int supla_rs_config_from_srv(supla_channel_t *ch, TSD_ChannelConfig *config)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    struct rs_nvs_state    *nvs = &data->nvs_state;
    int                     base_func;
    const int               ch_num = config->ChannelNumber;

    supla_log(LOG_INFO, "ch[%d] rs_ch got config from srv: fn=%d type=%d size=%d", ch_num,
              config->Func, config->ConfigType, config->ConfigSize);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    base_func = supla_rs_channel_get_base_function(ch);
    switch (base_func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER: {
        if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
            TChannelConfig_RollerShutter *rs_conf = (TChannelConfig_RollerShutter *)config->Config;
            supla_log(LOG_INFO, "roller shutter config: opening %dms closing %dms",
                      rs_conf->OpeningTimeMS, rs_conf->ClosingTimeMS);

            if (memcmp(rs_conf, &nvs->rs_conf, sizeof(nvs->rs_conf)) != 0) {
                supla_log(LOG_WARNING, "ch[%d] config altered - needs calibration", ch_num);
                nvs->active_func = config->Func;
                nvs->rs_conf = *rs_conf;
                data->calibration = true; //will need calibration
                supla_esp_nvs_channel_state_store(ch, &data->nvs_state, sizeof(data->nvs_state));
            }
        }
    } break;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND: {
        if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
            TChannelConfig_FacadeBlind *blinds_conf = (TChannelConfig_FacadeBlind *)config->Config;
            supla_log(LOG_INFO, "facade blind config: opening %dms closing %dms tilting %dms",
                      blinds_conf->OpeningTimeMS, blinds_conf->ClosingTimeMS,
                      blinds_conf->TiltingTimeMS);

            if (memcmp(blinds_conf, &nvs->blinds_conf, sizeof(nvs->blinds_conf)) != 0) {
                supla_log(LOG_WARNING, "ch[%d] config altered - needs calibration", ch_num);
                nvs->active_func = config->Func;
                nvs->blinds_conf = *blinds_conf;
                data->calibration = true; //will need calibration
                supla_esp_nvs_channel_state_store(ch, &data->nvs_state, sizeof(data->nvs_state));
            }
        }
    } break;
    default:
        break;
    }
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

static int supla_rs_on_calcfg_from_srv(supla_channel_t *ch, TSD_DeviceCalCfgRequest *calcfg)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    int                     rc = SUPLA_CALCFG_RESULT_FALSE;

    supla_log(LOG_INFO, "ch[%d] rs_ch got calcfg from srv: cmd=%d type=%d size=%d",
              calcfg->ChannelNumber, calcfg->Command, calcfg->DataType, calcfg->DataSize);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    switch (calcfg->Command) {
    case SUPLA_CALCFG_CMD_RECALIBRATE:
        data->calibration = true;
        rc = SUPLA_CALCFG_RESULT_IN_PROGRESS;
        break;
    default:
        break;
    }
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return rc;
}

supla_channel_t *supla_rs_channel_create(const struct rs_channel_config *config)
{
    const supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_RELAY,
        .supported_functions = config->supported_functions,
        .sync_values_onchange = true,
        .default_function = config->default_function,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE | SUPLA_CHANNEL_FLAG_RS_SBS_AND_STOP_ACTIONS,
        .on_channel_init = supla_rs_channel_init,
        .on_set_value = supla_rs_set,
        .on_calcfg_req = supla_rs_on_calcfg_from_srv,
        .on_config_set = supla_rs_config_to_srv,
        .on_config_recv = supla_rs_config_from_srv
    };

    const gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << config->gpio_open) | (1ULL << config->gpio_close),
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
    data->state = RS_STATE_IDLE;
    data->last_state = data->state;
    data->real_pos = -1;
    data->target_pos = data->real_pos;
    data->gpio_open = config->gpio_open;
    data->gpio_close = config->gpio_close;

    gpio_config(&gpio_conf);
    gpio_set_level(data->gpio_open, 0);
    gpio_set_level(data->gpio_close, 0);
    timer_args.arg = ch;

    esp_timer_create(&timer_args, &data->timer);
    esp_timer_start_periodic(data->timer, RS_TIMER_INTERVAL * 1000);
    return ch;
}

int supla_rs_channel_delete(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    esp_timer_delete(data->timer);
    free(data);
    return supla_channel_free(ch);
}

int supla_rs_channel_move_stop(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    supla_rs_channel_internal_stop(ch);
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

int supla_rs_channel_move_down(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    data->state = RS_STATE_CLOSING;
    data->target_pos = 100;
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

int supla_rs_channel_move_up(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    data->state = RS_STATE_OPENING;
    data->target_pos = 0;
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

int supla_rs_channel_move_down_or_stop(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    if (data->state != RS_STATE_IDLE) {
        supla_rs_channel_internal_stop(ch);
    } else {
        data->state = RS_STATE_CLOSING;
        data->target_pos = 100;
    }
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

int supla_rs_channel_move_up_or_stop(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    if (data->state != RS_STATE_IDLE) {
        supla_rs_channel_internal_stop(ch);
    } else {
        data->state = RS_STATE_OPENING;
        data->target_pos = 0;
    }
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

int supla_rs_channel_step_by_step(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    if (data->state != RS_STATE_IDLE) {
        supla_rs_channel_internal_stop(ch);
    } else if (data->last_state == RS_STATE_OPENING) {
        data->state = RS_STATE_CLOSING;
        data->target_pos = 100;
    } else if (data->last_state == RS_STATE_CLOSING) {
        data->state = RS_STATE_OPENING;
        data->target_pos = 0;
    } else if (data->real_pos < 50) {
        data->state = RS_STATE_CLOSING;
        data->target_pos = 100;
    } else {
        data->state = RS_STATE_OPENING;
        data->target_pos = 0;
    }
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}

int supla_rs_channel_set_target_position(supla_channel_t *ch, int8_t target)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    CHANNEL_SEMAPHORE_TAKE(data->mutex);
    data->target_pos = target;
    CHANNEL_SEMAPHORE_GIVE(data->mutex);
    return ESP_OK;
}
