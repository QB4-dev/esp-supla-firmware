#include "include/pir-sensor.h"

#include <stdlib.h>
#include <string.h>
#include <esp_timer.h>
#include <esp_log.h>

#define PIR_POLL_INTERVAL_US 10000   //10ms
#define DEAD_TIME_US         50000   //50ms
#define BUF_RESET_TIME_US   500000   //500ms
#define DETECT_EVENTS_MAX        5
#define CLICK_TIMEOUT_MIN      100   //ms
#define CLICK_TIMEOUT_MAX      250   //ms

struct pir_channel_data {
	gpio_num_t gpio;
	uint8_t level;
	esp_timer_handle_t timer;
	uint32_t detect_time;
	uint32_t idle_time;
	uint32_t detect_buf[DETECT_EVENTS_MAX];
	uint8_t ev_num;
	on_detect_calback_t on_detect_cb;
	void *cb_arg;
};

static void pir_sensor_poll(void *arg)
{
	const int click_actions[DETECT_EVENTS_MAX+1] = {
		[1] = SUPLA_ACTION_CAP_SHORT_PRESS_x1,
		[2] = SUPLA_ACTION_CAP_SHORT_PRESS_x2,
		[3] = SUPLA_ACTION_CAP_SHORT_PRESS_x3,
		[4] = SUPLA_ACTION_CAP_SHORT_PRESS_x4,
		[5] = SUPLA_ACTION_CAP_SHORT_PRESS_x5
	};
	supla_channel_t *ch = arg;
	supla_channel_config_t ch_config;
	int valid_clicks;
	struct pir_channel_data *pir_data = supla_channel_get_data(ch);
	supla_channel_get_config(ch,&ch_config);

	if(pir_data->level == 0 && pir_data->detect_time < DEAD_TIME_US)
	{
		// Dead time, ignore all
		pir_data->detect_time += PIR_POLL_INTERVAL_US;
		return;
	}

	pir_data->level = gpio_get_level(pir_data->gpio);
	if(pir_data->level == 0){
		// detection is active
		if(pir_data->detect_time == 0 && pir_data->on_detect_cb){
			//instant event
			pir_data->on_detect_cb(INPUT_EVENT_INIT,pir_data->cb_arg);
			supla_channel_emit_action(ch,SUPLA_ACTION_CAP_TURN_ON);
		}
		pir_data->detect_time += PIR_POLL_INTERVAL_US;
		pir_data->idle_time = 0;
	} else {
		if(pir_data->detect_time > 0){
			pir_data->detect_buf[pir_data->ev_num % DETECT_EVENTS_MAX] = pir_data->detect_time/1000;
			pir_data->ev_num++;
		}
		pir_data->idle_time += PIR_POLL_INTERVAL_US;
		pir_data->detect_time = 0;
	}
	if(pir_data->idle_time > BUF_RESET_TIME_US && pir_data->detect_buf[0] != 0){
		supla_log(LOG_INFO,"PIR sensor buf[%d | %d | %d | %d | %d]",
			pir_data->detect_buf[0],
			pir_data->detect_buf[1],
			pir_data->detect_buf[2],
			pir_data->detect_buf[3],
			pir_data->detect_buf[4]
		);

		valid_clicks = 0;
		for(int i=0; i < DETECT_EVENTS_MAX; i++){
			if( pir_data->detect_buf[i] >= CLICK_TIMEOUT_MIN && pir_data->detect_buf[i] <= CLICK_TIMEOUT_MAX )
				valid_clicks++;
		}
		if(valid_clicks){
			pir_data->on_detect_cb(valid_clicks,pir_data->cb_arg);
			if(ch_config.action_trigger_caps & click_actions[valid_clicks]){
				supla_log(LOG_INFO,"PIR sensor emit %d clicks",valid_clicks);
				supla_channel_emit_action(ch,click_actions[valid_clicks]);
			}
		}
		memset(pir_data->detect_buf,0,sizeof(pir_data->detect_buf));
		pir_data->ev_num = 0;
		pir_data->on_detect_cb(INPUT_EVENT_DONE,pir_data->cb_arg);
		supla_channel_emit_action(ch,SUPLA_ACTION_CAP_TURN_OFF);
	}
}

supla_channel_t *supla_pir_sensor_create(struct pir_sensor_config *pir_sensor_conf)
{
	supla_channel_config_t at_channel_config = {
		.type = SUPLA_CHANNELTYPE_ACTIONTRIGGER,
		.supported_functions = SUPLA_CHANNELFNC_ACTIONTRIGGER,
		.default_function = SUPLA_CHANNELFNC_ACTIONTRIGGER,
		.action_trigger_caps = pir_sensor_conf->action_trigger_caps,
		.action_trigger_related_channel = pir_sensor_conf->related_channel
	};
	gpio_config_t gpio_conf = {
		.pin_bit_mask = (1 << pir_sensor_conf->gpio),
		.mode = GPIO_MODE_INPUT,
		.intr_type = GPIO_INTR_DISABLE
	};
	esp_timer_create_args_t timer_args = {
		.name = "pir-sensor",
		.dispatch_method = ESP_TIMER_TASK,
		.callback = pir_sensor_poll,
	};
	struct pir_channel_data *pir_data;

	supla_channel_t *ch = supla_channel_create(&at_channel_config);
	if(!ch)
		return NULL;

	pir_data = calloc(1,sizeof(struct pir_channel_data));
	if(!pir_data){
		supla_channel_free(ch);
		return NULL;
	}
	pir_data->gpio = pir_sensor_conf->gpio;
	pir_data->level = 1;
	pir_data->on_detect_cb = pir_sensor_conf->on_detect_cb;
	pir_data->cb_arg = pir_sensor_conf->arg;
	supla_channel_set_data(ch,pir_data);

	gpio_config(&gpio_conf);
	timer_args.arg = ch;
	esp_timer_create(&timer_args,&pir_data->timer);
	esp_timer_start_periodic(pir_data->timer,PIR_POLL_INTERVAL_US);
	return ch;
}
