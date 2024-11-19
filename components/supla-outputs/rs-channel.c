#include "rs-channel.h"
#include <stdlib.h>
#include <string.h>
#include <driver/gpio.h>
#include <esp_timer.h>

enum rs_state { RS_STATE_IDLE, RS_STATE_MOVING_UP, RS_STATE_MOVING_DOWN };

struct rs_nvs_config {
    int active_func;
    union {
        TChannelConfig_RollerShutter rs;
    };
};

struct rs_channel_data {
    gpio_num_t           gpio_fwd;
    gpio_num_t           gpio_bck;
    uint32_t             closing_time;
    uint32_t             opening_time;
    enum rs_state        state;
    int8_t               current_pos; // 0 - closed; 100 - opened
    esp_timer_handle_t   timer;
    uint32_t             mseconds_left;
    struct rs_nvs_config nvs_config;
};

static int supla_rs_stop(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    data->state = RS_STATE_IDLE;
    gpio_set_level(data->gpio_fwd, 0);
    gpio_set_level(data->gpio_bck, 0);
    return ESP_OK;
}

static int supla_rs_move_up(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    data->state = RS_STATE_MOVING_UP;
    gpio_set_level(data->gpio_fwd, 1);
    gpio_set_level(data->gpio_bck, 0);
    return ESP_OK;
}

static int supla_rs_move_down(supla_channel_t *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    data->state = RS_STATE_MOVING_DOWN;
    gpio_set_level(data->gpio_fwd, 0);
    gpio_set_level(data->gpio_bck, 1);
    return ESP_OK;
}

static int supla_rs_set_position(supla_channel_t *ch, int8_t target)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);

    if (target == data->current_pos) {
        supla_rs_stop(ch);
    } else if (target < data->current_pos) {
        esp_timer_stop(data->timer);
        data->mseconds_left = abs(data->current_pos - target) * data->closing_time / 100;
        if (data->mseconds_left) {
            supla_rs_move_down(ch);
            esp_timer_start_periodic(data->timer, 1000 * 1000);
        }
    } else if (target > data->current_pos) {
        esp_timer_stop(data->timer);
        data->mseconds_left = abs(data->current_pos - target) * data->opening_time / 100;
        if (data->mseconds_left) {
            supla_rs_move_up(ch);
            esp_timer_start_periodic(data->timer, 1000 * 1000);
        }
    }
    return ESP_OK;
}

static int supla_rs_set(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    TDSC_RollerShutterValue rs_ret = {};

    char task = new_value->value[0];
    char tilt = new_value->value[1];

    uint32_t open = ((new_value->DurationMS >> 0) & 0xFFFF) * 100;
    uint32_t close = ((new_value->DurationMS >> 16) & 0xFFFF) * 100;

    data->opening_time = open;
    data->closing_time = close;

    rs_ret.position = data->current_pos;

    supla_log(LOG_INFO, "rs set task=%d pos=%d with open=%d close=%d", task, open, close);

    switch (task) {
    case 0: { // STOP
        esp_timer_stop(data->timer);
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
        if (task >= 10 && task <= 110) {
            supla_rs_set_position(ch, task - 10);
        }
        break;
    }
    }

    return supla_channel_set_roller_shutter_value(ch, &rs_ret);
}

static int supla_srv_rs_config(supla_channel_t *ch, TSD_ChannelConfig *config)
{
    int active_func;
    supla_channel_get_active_function(ch, &active_func);

    supla_log(LOG_INFO, "got relay config fn=%d size=%d", active_func, config->ConfigSize);

    switch (active_func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER: {
        if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
            TChannelConfig_RollerShutter *rs_conf = (TChannelConfig_RollerShutter *)config->Config;
            supla_log(LOG_INFO, "roller shutter config: opening %dms closing %dms",
                      rs_conf->OpeningTimeMS, rs_conf->ClosingTimeMS);
        }
    } break;
    default:
        break;
    }
    return ESP_OK;
}

static void rs_timer_event(void *ch)
{
    struct rs_channel_data *data = supla_channel_get_data(ch);
    TDSC_RollerShutterValue rs_ret = {};

    data->mseconds_left -= 1000;
    switch (data->state) {
    case RS_STATE_MOVING_UP:
        data->current_pos += 1;
        break;
    case RS_STATE_MOVING_DOWN:
        data->current_pos -= 1;
        break;
    default:
        break;
    }

    if (data->mseconds_left > 0) {
        supla_log(LOG_INFO, "rs pos=%d %dms", data->current_pos, data->mseconds_left);
    } else {
        esp_timer_stop(data->timer);
        supla_rs_stop(ch);
    }

    rs_ret.position = data->current_pos;
    supla_channel_set_roller_shutter_value(ch, &rs_ret);
}

supla_channel_t *supla_rs_channel_create(const struct rs_channel_config *config)
{
    const supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_RELAY,
        .supported_functions = config->supported_functions,
        .default_function = config->default_function,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE | SUPLA_CHANNEL_FLAG_RS_SBS_AND_STOP_ACTIONS |
                 SUPLA_CHANNEL_FLAG_CALCFG_RECALIBRATE |
                 SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE,
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
    data->gpio_fwd = config->gpio_fwd;
    data->gpio_bck = config->gpio_bck;

    gpio_config(&gpio_conf);
    gpio_set_level(data->gpio_fwd, 0);
    gpio_set_level(data->gpio_bck, 0);

    timer_args.arg = ch;
    esp_timer_create(&timer_args, &data->timer);
    return ch;
}
