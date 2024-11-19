#include "relay-channel.h"
#include <stdlib.h>
#include <string.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp-supla.h>

struct relay_nvs_config {
    int active_func;
    union {
        TChannelConfig_PowerSwitch    power_switch;
        TChannelConfig_StaircaseTimer staircase_timer;
    };
};

struct relay_channel_data {
    gpio_num_t              gpio;
    esp_timer_handle_t      timer;
    uint32_t                mseconds_left;
    struct relay_nvs_config nvs_config;
};

static int supla_relay_channel_init(supla_channel_t *ch)
{
    struct relay_channel_data *data = supla_channel_get_data(ch);
    const int                  ch_num = supla_channel_get_assigned_number(ch);

    supla_log(LOG_INFO, "ch[%d] relay_ch init", ch_num);
    supla_esp_nvs_channel_config_restore(ch, &data->nvs_config, sizeof(data->nvs_config));
    supla_log(LOG_INFO, "ch[%d] relay_ch: func=%d", ch_num, data->nvs_config.active_func);
    return SUPLA_RESULTCODE_TRUE;
}

static int supla_srv_relay_config(supla_channel_t *ch, TSD_ChannelConfig *config)
{
    struct relay_channel_data *data = supla_channel_get_data(ch);
    supla_log(LOG_INFO, "ch[%d] relay_ch got config fn=%d", config->ChannelNumber, config->Func);

    switch (config->Func) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK: {
        if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
        }
    } break;
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGATE:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR: {
        if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
        }
    } break;
    case SUPLA_CHANNELFNC_POWERSWITCH:
    case SUPLA_CHANNELFNC_LIGHTSWITCH: {
        if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
            TChannelConfig_PowerSwitch *switch_conf = (TChannelConfig_PowerSwitch *)config->Config;
            supla_log(LOG_INFO, "power switch config: oc-max %d oc-tresh %d",
                      switch_conf->OvercurrentMaxAllowed, switch_conf->OvercurrentThreshold);

            data->nvs_config.active_func = config->Func;
            data->nvs_config.power_switch = *switch_conf;
            supla_esp_nvs_channel_config_store(ch, &data->nvs_config, sizeof(data->nvs_config));
        }
    } break;
    case SUPLA_CHANNELFNC_STAIRCASETIMER: {
        if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
            TChannelConfig_StaircaseTimer *staircase_conf =
                (TChannelConfig_StaircaseTimer *)config->Config;
            supla_log(LOG_INFO, "staircase config: %dms", staircase_conf->TimeMS);

            data->nvs_config.active_func = config->Func;
            data->nvs_config.staircase_timer = *staircase_conf;
            supla_esp_nvs_channel_config_store(ch, &data->nvs_config, sizeof(data->nvs_config));
        }
    } break;
    default:
        break;
    }
    return ESP_OK;
}

static int supla_relay_channel_set(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    TRelayChannel_Value       *relay_val = (TRelayChannel_Value *)new_value->value;
    TTimerState_ExtendedValue  timer_state = {};
    const int                  ch_num = supla_channel_get_assigned_number(ch);
    struct relay_channel_data *data = supla_channel_get_data(ch);

    esp_timer_stop(data->timer);
    data->mseconds_left = relay_val->hi ? (new_value->DurationMS) : 0;
    if (data->mseconds_left) {
        esp_timer_start_periodic(data->timer, 100 * 1000);
    } else {
        supla_channel_set_timer_state_extvalue(ch, &timer_state);
    }

    gpio_set_level(data->gpio, relay_val->hi);
    if (data->mseconds_left) {
        supla_log(LOG_INFO, "ch[%d] relay set %s for %dms", ch_num, relay_val->hi ? "ON" : "OFF",
                  data->mseconds_left);
    } else {
        supla_log(LOG_INFO, "ch[%d] relay set %s", ch_num, relay_val->hi ? "ON" : "OFF");
    }
    return supla_channel_set_relay_value(ch, relay_val);
}

static void countdown_timer_event(void *ch)
{
    TSD_SuplaChannelNewValue   zero_value = {};
    TTimerState_ExtendedValue  timer_state = {};
    struct relay_channel_data *data = supla_channel_get_data(ch);

    data->mseconds_left -= 100;
    timer_state.RemainingTimeMs = data->mseconds_left;

    if (data->mseconds_left > 0) {
        supla_channel_set_timer_state_extvalue(ch, &timer_state);
    } else {
        supla_relay_channel_set(ch, &zero_value);
    }
}

supla_channel_t *supla_relay_channel_create(const struct relay_channel_config *config)
{
    const supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_RELAY,
        .supported_functions = config->supported_functions,
        .default_function = config->default_function,
        .default_caption = "RELAY",
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE | SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED,
        .on_channel_init = supla_relay_channel_init,
        .on_set_value = supla_relay_channel_set,
        .on_config_recv = supla_srv_relay_config
    };

    const gpio_config_t gpio_conf = {
        .pin_bit_mask = (1 << config->gpio),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE //
    };

    esp_timer_create_args_t timer_args = {
        .name = "relay-off",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = countdown_timer_event,
    };
    struct relay_channel_data *data;

    if ((config->supported_functions | RELAY_CH_SUPPORTED_FUNC_BITS) !=
        RELAY_CH_SUPPORTED_FUNC_BITS) {
        supla_log(LOG_ERR, "relay_ch supported_functions conflict %x != %x",
                  config->supported_functions, RELAY_CH_SUPPORTED_FUNC_BITS);
        return NULL;
    }

    switch (config->default_function) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGATEWAYLOCK:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEDOORLOCK:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGATE:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEGARAGEDOOR:
    case SUPLA_CHANNELFNC_POWERSWITCH:
    case SUPLA_CHANNELFNC_LIGHTSWITCH:
    case SUPLA_CHANNELFNC_STAIRCASETIMER:
        break;
    default:
        supla_log(LOG_ERR, "channel default_function %d not supported", config->default_function);
        return NULL;
    }

    supla_channel_t *ch = supla_channel_create(&supla_channel_config);
    if (!ch)
        return NULL;

    data = calloc(1, sizeof(struct relay_channel_data));
    if (!data) {
        supla_channel_free(ch);
        return NULL;
    }

    supla_channel_set_data(ch, data);
    data->gpio = config->gpio;

    gpio_config(&gpio_conf);
    gpio_set_level(data->gpio, 0);

    timer_args.arg = ch;
    esp_timer_create(&timer_args, &data->timer);
    return ch;
}

int supla_relay_channel_delete(supla_channel_t *ch)
{
    struct relay_channel_data *data = supla_channel_get_data(ch);
    esp_timer_delete(data->timer);
    free(data);
    return supla_channel_free(ch);
}

int supla_relay_channel_get_state(supla_channel_t *ch)
{
    struct relay_channel_data *data = supla_channel_get_data(ch);
    return gpio_get_level(data->gpio);
}

int supla_relay_channel_set_local(supla_channel_t *ch, TRelayChannel_Value *relay_value)
{
    int                        active_function;
    struct relay_channel_data *data = supla_channel_get_data(ch);
    TSD_SuplaChannelNewValue   new_value = {};
    supla_channel_get_active_function(ch, &active_function);

    memcpy(&new_value.value, relay_value, sizeof(TRelayChannel_Value));
    if (active_function == SUPLA_CHANNELFNC_STAIRCASETIMER && relay_value->hi)
        new_value.DurationMS = data->nvs_config.staircase_timer.TimeMS;

    return supla_relay_channel_set(ch, &new_value);
}
