#include "include/ledc-channel.h"

#include <stdlib.h>
#include <esp_log.h>
#include <esp_timer.h>

struct ledc_channel_data {
    ledc_channel_config_t ledc;
    uint32_t fade_time;
    uint32_t duty_res;
    esp_timer_handle_t timer;
};

static void deferred_fade_out(void *ch)
{
    TSD_SuplaChannelNewValue new_value = {};
    supla_ledc_channel_set_brightness(ch,&new_value);
}

int supla_ledc_channel_set_brightness(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    struct ledc_channel_data *ch_data = supla_channel_get_data(ch);
    TRGBW_Value *rgbw = (TRGBW_Value*)new_value->value;
    uint32_t fade_time = ch_data->fade_time;
    uint32_t duty = (rgbw->brightness * ch_data->duty_res)/100;

    esp_timer_stop(ch_data->timer);
    ledc_set_fade_with_time(ch_data->ledc.speed_mode,ch_data->ledc.channel,duty,fade_time);
    ledc_fade_start(ch_data->ledc.speed_mode,ch_data->ledc.channel,LEDC_FADE_NO_WAIT);

    if(new_value->DurationMS)
        esp_timer_start_once(ch_data->timer,new_value->DurationMS * 1000);

    return supla_channel_set_rgbw_value(ch,rgbw);
}

supla_channel_t *supla_ledc_channel_create(const struct ledc_channel_config *ledc_ch_conf)
{
    supla_channel_config_t dimmer_channel_config = {
        .type = SUPLA_CHANNELTYPE_DIMMER,
        .supported_functions = SUPLA_BIT_FUNC_STAIRCASETIMER,
        .default_function = SUPLA_CHANNELFNC_DIMMER,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE|SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED,
        .on_set_value = supla_ledc_channel_set_brightness
    };
    ledc_timer_config_t ledc_timer_conf = {
        .timer_num = LEDC_TIMER_0   ,         // timer index
        .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
        .speed_mode = LEDC_LOW_SPEED_MODE,   // timer mode
        .freq_hz = 200,                      // frequency of PWM signal
    };
    esp_timer_create_args_t timer_args = {
        .name = "ledc-off",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = deferred_fade_out,
    };
    struct ledc_channel_data *ch_data;

    supla_channel_t *ch = supla_channel_create(&dimmer_channel_config);
    if(!ch)
        return NULL;

    ch_data = malloc(sizeof(struct ledc_channel_data));
    if(!ch_data){
        supla_channel_free(ch);
        return NULL;
    }

    ch_data->ledc.gpio_num = ledc_ch_conf->gpio;
    ch_data->ledc.speed_mode = LEDC_LOW_SPEED_MODE;
    ch_data->ledc.channel = ledc_ch_conf->ledc_channel;
    ch_data->ledc.duty = 0;

    //fade_time can't be less than 10
    ch_data->fade_time = ledc_ch_conf->fade_time ? ledc_ch_conf->fade_time : 10;
    ch_data->duty_res = (1 << ledc_timer_conf.duty_resolution)-1;

    supla_channel_set_data(ch,ch_data);
    ledc_timer_config(&ledc_timer_conf);
    ledc_channel_config(&ch_data->ledc);
    ledc_fade_func_install(0);
    timer_args.arg = ch;
    esp_timer_create(&timer_args,&ch_data->timer);
    return ch;
}
