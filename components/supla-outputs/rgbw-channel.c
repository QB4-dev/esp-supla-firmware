#include "rgbw-channel.h"
#include <stdlib.h>
#include <esp_log.h>
#include <esp_timer.h>

struct rgbw_channel_data {
    esp_timer_handle_t timer;
};

static void deferred_fade_out(void *ch)
{
    TSD_SuplaChannelNewValue new_value = {};
    supla_rgbw_channel_set_value(ch, &new_value);
}

int supla_rgbw_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    struct rgbw_channel_data *ch_data = supla_channel_get_data(ch);
    TRGBW_Value              *rgbw = (TRGBW_Value *)new_value->value;

    esp_timer_stop(ch_data->timer);

    supla_log(LOG_INFO, "new RGBW val: R=%d G=%d B=%d CB=%d W=%d", rgbw->R, rgbw->G, rgbw->B,
              rgbw->colorBrightness, rgbw->brightness);

    if (new_value->DurationMS)
        esp_timer_start_once(ch_data->timer, new_value->DurationMS * 1000);

    return supla_channel_set_rgbw_value(ch, rgbw);
}

supla_channel_t *supla_ext_rgbw_channel_create(const struct rgbw_channel_config *ch_conf)
{
    supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_DIMMERANDRGBLED,
        .supported_functions = 0xFFFF,
        .default_function = SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE | SUPLA_CHANNEL_FLAG_RGBW_COMMANDS_SUPPORTED,
        .on_set_value = supla_rgbw_channel_set_value
    };
    esp_timer_create_args_t timer_args = {
        .name = "ledc-off",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = deferred_fade_out,
    };
    struct rgbw_channel_data *ch_data;

    supla_channel_t *ch = supla_channel_create(&supla_channel_config);
    if (!ch)
        return NULL;

    ch_data = malloc(sizeof(struct rgbw_channel_data));
    if (!ch_data) {
        supla_channel_free(ch);
        return NULL;
    }

    supla_channel_set_data(ch, ch_data);
    timer_args.arg = ch;
    esp_timer_create(&timer_args, &ch_data->timer);
    return ch;
}
