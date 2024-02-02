#include "include/exp-input.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stdlib.h>
#include <string.h>
#include <esp_timer.h>
#include <esp_log.h>

#define POLL_INTERVAL_US    100000   //100ms

struct exp_input {
    i2c_dev_t *i2c_exp;
    gpio_num_t pin_num;
    exp_input_active_t active_level;
    uint32_t prev_level;
    esp_timer_handle_t timer;
    TickType_t init_tick;
    bool hold;
    uint32_t hold_timeout;
    on_exp_event_calback_t callback;
    void *cb_arg;
};

static void exp_input_poll(void *arg)
{
    TickType_t tick = xTaskGetTickCount();
    struct exp_input *input = arg;
    uint32_t level;
    exp_input_event_t event = EXP_INPUT_EVENT_NONE;

	pca9557_get_level(input->i2c_exp,input->pin_num,&level);
	if(input->prev_level && !level){
	    event = (input->active_level == ACTIVE_LOW) ?
	            EXP_INPUT_EVENT_INIT : EXP_INPUT_EVENT_DONE;
	    input->callback(input->pin_num,event,input->cb_arg);
	}

    if(!input->prev_level && level){
        event = (input->active_level == ACTIVE_HIGH) ?
                EXP_INPUT_EVENT_INIT : EXP_INPUT_EVENT_DONE;
        input->callback(input->pin_num,event,input->cb_arg);
    }
    if(event == EXP_INPUT_EVENT_INIT){
        input->init_tick = xTaskGetTickCount();
        input->hold = false;
    }

    if(level == input->active_level && !input->hold && (tick - input->init_tick) > pdMS_TO_TICKS(input->hold_timeout) ){
        input->callback(input->pin_num,EXP_INPUT_EVENT_HOLD,input->cb_arg);
        input->hold = true;
    }

	input->prev_level = level;
}

exp_input_t exp_input_create(const struct exp_input_config *input_conf)
{
	esp_timer_create_args_t timer_args = {
		.name = "exp-input",
		.dispatch_method = ESP_TIMER_TASK,
		.callback = exp_input_poll,
	};
	struct exp_input *input;

	input = calloc(1,sizeof(struct exp_input));
	if(!input)
		return NULL;

	input->i2c_exp = input_conf->i2c_expander;
    input->pin_num = input_conf->pin_num;
	input->callback = input_conf->callback;
	input->cb_arg = input_conf->arg;
	input->active_level = input_conf->active_level;
    input->hold_timeout = input_conf->hold_timeout ? input_conf->hold_timeout : 3000;

	pca9557_set_mode(input->i2c_exp,input->pin_num,PCA9557_MODE_INPUT);
	pca9557_get_level(input->i2c_exp,input->pin_num,&input->prev_level);

	timer_args.arg = input;
	esp_timer_create(&timer_args,&input->timer);
	esp_timer_start_periodic(input->timer,POLL_INTERVAL_US);
	return input;
}
