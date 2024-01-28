#include "include/click-input.h"

#include <stdlib.h>
#include <string.h>
#include <esp_timer.h>
#include <esp_log.h>

#define PIR_POLL_INTERVAL_US 10000   //10ms
#define DEAD_TIME_US         50000   //50ms
#define BUF_RESET_TIME_US   500000   //500ms
#define CLICK_EVENTS_MAX        5
#define CLICK_MIN       75   //ms
#define CLICK_MAX      250   //ms

struct click_input_channel_data {
	gpio_num_t gpio;
	uint8_t level;
	esp_timer_handle_t timer;
	uint32_t init_time;
	uint32_t idle_time;
	uint32_t buf[CLICK_EVENTS_MAX];
	uint8_t ev_num;
	on_detect_calback_t on_detect_cb;
	void *cb_arg;
};

static void click_input_poll(void *arg)
{
	const int click_actions[CLICK_EVENTS_MAX+1] = {
		[1] = SUPLA_ACTION_CAP_SHORT_PRESS_x1,
		[2] = SUPLA_ACTION_CAP_SHORT_PRESS_x2,
		[3] = SUPLA_ACTION_CAP_SHORT_PRESS_x3,
		[4] = SUPLA_ACTION_CAP_SHORT_PRESS_x4,
		[5] = SUPLA_ACTION_CAP_SHORT_PRESS_x5
	};
	supla_channel_t *ch = arg;
	supla_channel_config_t ch_config;
	int valid_clicks;
	struct click_input_channel_data *ch_data = supla_channel_get_data(ch);
	supla_channel_get_config(ch,&ch_config);

	if(ch_data->level == 0 && ch_data->init_time < DEAD_TIME_US){
		// Dead time, ignore all
		ch_data->init_time += PIR_POLL_INTERVAL_US;
		return;
	}

	ch_data->level = gpio_get_level(ch_data->gpio);
	if(ch_data->level == 0){
		// detection is active
		if(ch_data->init_time == 0 && ch_data->on_detect_cb){
			//instant event
			ch_data->on_detect_cb(INPUT_EVENT_INIT,ch_data->cb_arg);
			//supla_channel_emit_action(ch,SUPLA_ACTION_CAP_TURN_ON);
		}
		ch_data->init_time += PIR_POLL_INTERVAL_US;
		ch_data->idle_time = 0;
	} else {
		if(ch_data->init_time > 0){
			ch_data->buf[ch_data->ev_num % CLICK_EVENTS_MAX] = ch_data->init_time/1000;
			ch_data->ev_num++;
		}
		ch_data->idle_time += PIR_POLL_INTERVAL_US;
		ch_data->init_time = 0;
	}
	if(ch_data->idle_time > BUF_RESET_TIME_US && ch_data->buf[0] != 0){
		supla_log(LOG_INFO,"click buf[%d | %d | %d | %d | %d]",
			ch_data->buf[0],
			ch_data->buf[1],
			ch_data->buf[2],
			ch_data->buf[3],
			ch_data->buf[4]
		);

		valid_clicks = 0;
		for(int i=0; i < CLICK_EVENTS_MAX; i++){
			if( ch_data->buf[i] >= CLICK_MIN && ch_data->buf[i] <= CLICK_MAX )
				valid_clicks++;
		}
		if(valid_clicks){
			ch_data->on_detect_cb(valid_clicks,ch_data->cb_arg);
			if(ch_config.action_trigger_caps & click_actions[valid_clicks])
				supla_channel_emit_action(ch,click_actions[valid_clicks]);
		}
		memset(ch_data->buf,0,sizeof(ch_data->buf));
		ch_data->ev_num = 0;
		ch_data->on_detect_cb(INPUT_EVENT_DONE,ch_data->cb_arg);
		//supla_channel_emit_action(ch,SUPLA_ACTION_CAP_TURN_OFF);
	}
}

supla_channel_t *supla_click_input_create(const struct click_input_config *input_conf)
{
	supla_channel_config_t at_channel_config = {
		.type = SUPLA_CHANNELTYPE_ACTIONTRIGGER,
		.supported_functions = SUPLA_CHANNELFNC_ACTIONTRIGGER,
		.default_function = SUPLA_CHANNELFNC_ACTIONTRIGGER,
		.action_trigger_caps = input_conf->action_trigger_caps,
		.action_trigger_related_channel = input_conf->related_channel
	};
	gpio_config_t gpio_conf = {
		.pin_bit_mask = (1 << input_conf->gpio),
		.mode = GPIO_MODE_INPUT,
		.intr_type = GPIO_INTR_DISABLE
	};
	esp_timer_create_args_t timer_args = {
		.name = "pir-sensor",
		.dispatch_method = ESP_TIMER_TASK,
		.callback = click_input_poll,
	};
	struct click_input_channel_data *pir_data;

	supla_channel_t *ch = supla_channel_create(&at_channel_config);
	if(!ch)
		return NULL;

	pir_data = calloc(1,sizeof(struct click_input_channel_data));
	if(!pir_data){
		supla_channel_free(ch);
		return NULL;
	}
	pir_data->gpio = input_conf->gpio;
	pir_data->level = 1;
	pir_data->on_detect_cb = input_conf->on_detect_cb;
	pir_data->cb_arg = input_conf->arg;
	supla_channel_set_data(ch,pir_data);

	gpio_config(&gpio_conf);
	timer_args.arg = ch;
	esp_timer_create(&timer_args,&pir_data->timer);
	esp_timer_start_periodic(pir_data->timer,PIR_POLL_INTERVAL_US);
	return ch;
}
