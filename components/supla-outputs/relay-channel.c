#include "relay-channel.h"
#include <stdlib.h>
#include <string.h>
#include <driver/gpio.h>
#include <esp_timer.h>

struct relay_channel_data {
    gpio_num_t         gpio;
    uint32_t           seconds_left;
    esp_timer_handle_t timer;
};

static int supla_relay_set(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    struct relay_channel_data *data = supla_channel_get_data(ch);
    TRelayChannel_Value       *relay_val = (TRelayChannel_Value *)new_value->value;
    TTimerState_ExtendedValue  timer_state = {};

    esp_timer_stop(data->timer);
    gpio_set_level(data->gpio, !relay_val->hi);
    data->seconds_left = new_value->DurationMS / 1000;

    if (new_value->DurationMS) {
        supla_log(LOG_INFO, "relay set value %s for %ds", relay_val->hi ? "ON" : "OFF",
                  data->seconds_left);
        esp_timer_start_periodic(data->timer, 1000 * 1000);
    } else {
        supla_log(LOG_INFO, "relay set value %s", relay_val->hi ? "ON" : "OFF");
        supla_channel_set_timer_state_extvalue(ch, &timer_state);
    }
    return supla_channel_set_relay_value(ch, relay_val);
}

static int supla_relay_config(supla_channel_t *ch, TSD_ChannelConfig *config)
{
    int active_func;

    supla_channel_get_active_function(ch, &active_func);

    switch (active_func) {
    case SUPLA_CHANNELFNC_STAIRCASETIMER: {
        TChannelConfig_StaircaseTimer *timer_conf = (TChannelConfig_StaircaseTimer *)config->Config;

        supla_log(LOG_INFO, "staircase config: %dms", timer_conf->TimeMS);
    } break;
    default:
        break;
    }

    return ESP_OK;
}

static void countdown_timer_event(void *ch)
{
    TSD_SuplaChannelNewValue   zero_value = {};
    TTimerState_ExtendedValue  timer_state = {};
    struct relay_channel_data *data = supla_channel_get_data(ch);

    data->seconds_left--;
    supla_log(LOG_DEBUG, "relay countdown timer %d s left", data->seconds_left);

    timer_state.RemainingTimeMs = data->seconds_left * 1000;

    if (data->seconds_left > 0) {
        supla_channel_set_timer_state_extvalue(ch, &timer_state);
    } else {
        esp_timer_stop(data->timer);
        supla_relay_set(ch, &zero_value);
    }
}

supla_channel_t *supla_relay_channel_create(const struct relay_channel_config *config)
{
    const supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_RELAY,
        .supported_functions = config->supported_functions,
        .default_function = config->default_function,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE | SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED,
        .on_set_value = supla_relay_set,
        .on_config_recv = supla_relay_config
    };

    const gpio_config_t gpio_conf = { .pin_bit_mask = (1 << config->gpio),
                                      .mode = GPIO_MODE_OUTPUT,
                                      .intr_type = GPIO_INTR_DISABLE };

    esp_timer_create_args_t timer_args = {
        .name = "relay-off",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = countdown_timer_event,
    };
    struct relay_channel_data *data;

    supla_channel_t *ch = supla_channel_create(&supla_channel_config);
    if (!ch)
        return NULL;

    data = malloc(sizeof(struct relay_channel_data));
    if (!data) {
        supla_channel_free(ch);
        return NULL;
    }

    supla_channel_set_data(ch, data);
    data->gpio = config->gpio;

    gpio_config(&gpio_conf);

    timer_args.arg = ch;
    esp_timer_create(&timer_args, &data->timer);
    return ch;
}
