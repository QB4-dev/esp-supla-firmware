#include "include/relay-channel.h"

#include <stdlib.h>
#include <string.h>
#include <driver/gpio.h>
#include <esp_timer.h>

struct relay_channel_data {
    gpio_num_t gpio;
    uint32_t seconds_left;
    esp_timer_handle_t timer;
};

static int relay_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    struct relay_channel_data *data = supla_channel_get_data(ch);
    TRelayChannel_Value *relay_val = (TRelayChannel_Value*)new_value->value;

    esp_timer_stop(data->timer);
    gpio_set_level(data->gpio,!relay_val->hi);

    if(new_value->DurationMS){
        data->seconds_left = new_value->DurationMS / 1000;
        esp_timer_start_periodic(data->timer,1000*1000);
        supla_log(LOG_INFO,"relay set value %s for %dms", relay_val->hi ? "ON" : "OFF",new_value->DurationMS);
    } else {
        supla_log(LOG_INFO,"relay set value %s", relay_val->hi ? "ON" : "OFF");
    }

    return supla_channel_set_relay_value(ch,relay_val);
}

static void countdown_timer_event(void *ch)
{
    TSD_SuplaChannelNewValue new_value = {};
    TTimerState_ExtendedValue timer_state = {};
    struct relay_channel_data *data = supla_channel_get_data(ch);

    data->seconds_left--;
    supla_log(LOG_DEBUG,"relay countdown timer %d s left",data->seconds_left);

    timer_state.RemainingTimeMs = data->seconds_left * 1000;

    if(data->seconds_left > 0){
        supla_channel_set_timer_state_extvalue(ch,&timer_state);
    } else {
        esp_timer_stop(data->timer);
        relay_set_value(ch,&new_value);
    }
}

supla_channel_t *supla_relay_channel_create(const struct relay_channel_config *config)
{
    supla_channel_config_t relay_channel_config = {
        .type = SUPLA_CHANNELTYPE_RELAY,
        .supported_functions = 0xFF,
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE|SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED,
        .on_set_value = relay_set_value
    };
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1 << config->gpio),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_timer_create_args_t timer_args = {
        .name = "relay-off",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = countdown_timer_event,
    };
    struct relay_channel_data *data;

    supla_channel_t *ch = supla_channel_create(&relay_channel_config);
    if(!ch)
        return NULL;

    data = malloc(sizeof(struct relay_channel_data));
    if(!data){
        supla_channel_free(ch);
        return NULL;
    }

    supla_channel_set_data(ch,data);
    data->gpio = config->gpio;

    gpio_config(&gpio_conf);

    timer_args.arg = ch;
    esp_timer_create(&timer_args,&data->timer);
    return ch;
}
