#include "include/ledc-channel.h"

#include <stdlib.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <driver/ledc.h>

#define LEDC_DUTY_LIMIT       8192 //2^13

struct ledc_channel_data {
	ledc_channel_config_t ledc;
	uint32_t fade_time;
	uint32_t duty;
	esp_timer_handle_t timer;
};

static void deferred_fade_out(void *arg)
{
	supla_channel_t *ch = arg;

	TSD_SuplaChannelNewValue new_value = {};
	TRGBW_Value *rgbw = (TRGBW_Value*)&new_value.value;
	rgbw->brightness = 0;
	supla_ledc_channel_set_brightness(ch,&new_value);
	supla_log(LOG_INFO,"LEDC deferred off");
}

int supla_ledc_channel_set_brightness(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
	struct ledc_channel_data *ch_data = supla_channel_get_data(ch);
	TRGBW_Value *rgbw = (TRGBW_Value*)new_value->value;
	uint32_t old_duty = ch_data->duty;
	uint32_t fade_val;
	ch_data->duty = rgbw->brightness * LEDC_DUTY_LIMIT/100;
	fade_val = (ch_data->fade_time * abs(old_duty - ch_data->duty)/LEDC_DUTY_LIMIT);
	fade_val = (fade_val > 10) ? fade_val : 10; //cannot be less than 10

	supla_log(LOG_INFO,"LEDC set brightness %u fade_duty = %d",(uint8_t)rgbw->brightness,fade_val);
	ledc_set_fade_with_time(ch_data->ledc.speed_mode,ch_data->ledc.channel,ch_data->duty,fade_val);
	ledc_fade_start(ch_data->ledc.speed_mode,ch_data->ledc.channel, LEDC_FADE_NO_WAIT);

	esp_timer_stop(ch_data->timer);
	if(new_value->DurationMS)
		esp_timer_start_once(ch_data->timer,new_value->DurationMS * 1000);
	return supla_channel_set_rgbw_value(ch,rgbw);
}

supla_channel_t *supla_ledc_channel_create(gpio_num_t gpio, uint32_t fade_ms)
{
	supla_channel_config_t dimmer_channel_config = {
		.type = SUPLA_CHANNELTYPE_DIMMER,
		.supported_functions = SUPLA_BIT_FUNC_STAIRCASETIMER,
		.default_function = SUPLA_CHANNELFNC_DIMMER,
		.flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
		.on_set_value = supla_ledc_channel_set_brightness
	};
	ledc_timer_config_t ledc_timer = {
		.timer_num = LEDC_TIMER_0   ,         // timer index
		.duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
		.speed_mode = LEDC_HIGH_SPEED_MODE,   // timer mode
		.freq_hz = 1000,                      // frequency of PWM signal
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

	ch_data->duty = 0;
	ch_data->fade_time = fade_ms ? fade_ms : 1;

	ch_data->ledc.gpio_num = gpio;
	ch_data->ledc.speed_mode = LEDC_LOW_SPEED_MODE;
	ch_data->ledc.channel = LEDC_CHANNEL_0;
	ch_data->ledc.duty = 0;

	supla_channel_set_data(ch,ch_data);
	ledc_timer_config(&ledc_timer);
	ledc_channel_config(&ch_data->ledc);
	ledc_fade_func_install(0);
	timer_args.arg = ch;
	esp_timer_create(&timer_args,&ch_data->timer);
	return ch;
}
