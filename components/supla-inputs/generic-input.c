#include <generic-input.h>
#include <stdlib.h>
#include <string.h>
#include <esp_timer.h>
#include <esp_log.h>

#define DEFAULT_POLL_INTERVAL_US 10000 //10ms
#define DEAD_TIME_US 50000             //50ms
#define BUF_RESET_TIME_US 500000       //500ms
#define CLICK_EVENTS_MAX 5
#define CLICK_MIN 100 //ms
#define CLICK_MAX 250 //ms

struct input_data {
    gpio_num_t         gpio;
    uint8_t            level;
    esp_timer_handle_t timer;
    uint32_t           init_time;
    uint32_t           idle_time;
    uint32_t           buf[CLICK_EVENTS_MAX];
    uint8_t            ev_num;
    on_input_calback_t on_detect_cb;
    void              *cb_arg;
};

static void input_poll(void *arg)
{
    supla_channel_t       *ch = arg;
    supla_channel_config_t ch_config;
    int                    valid_clicks;
    struct input_data     *data;

    const int click_actions[CLICK_EVENTS_MAX + 1] = {
        [1] = SUPLA_ACTION_CAP_SHORT_PRESS_x1,
        [2] = SUPLA_ACTION_CAP_SHORT_PRESS_x2,
        [3] = SUPLA_ACTION_CAP_SHORT_PRESS_x3,
        [4] = SUPLA_ACTION_CAP_SHORT_PRESS_x4,
        [5] = SUPLA_ACTION_CAP_SHORT_PRESS_x5 //
    };

    data = supla_channel_get_data(ch);
    supla_channel_get_config(ch, &ch_config);

    if (data->level == 0 && data->init_time < DEAD_TIME_US) {
        // Dead time, ignore all
        data->init_time += DEFAULT_POLL_INTERVAL_US;
        return;
    }

    data->level = gpio_get_level(data->gpio);
    if (data->level == 0) {
        // detection is active
        if (data->init_time == 0 && data->on_detect_cb) {
            //instant event
            data->on_detect_cb(data->gpio, INPUT_EVENT_INIT, data->cb_arg);
            //supla_channel_emit_action(ch,SUPLA_ACTION_CAP_TURN_ON);
        }
        data->init_time += DEFAULT_POLL_INTERVAL_US;
        data->idle_time = 0;
    } else {
        if (data->init_time > 0) {
            data->buf[data->ev_num % CLICK_EVENTS_MAX] = data->init_time / 1000;
            data->ev_num++;
        }
        data->idle_time += DEFAULT_POLL_INTERVAL_US;
        data->init_time = 0;
    }
    if (data->idle_time > BUF_RESET_TIME_US && data->buf[0] != 0) {
        supla_log(LOG_INFO, "click buf[%d | %d | %d | %d | %d]", data->buf[0], data->buf[1],
                  data->buf[2], data->buf[3], data->buf[4]);

        valid_clicks = 0;
        for (int i = 0; i < CLICK_EVENTS_MAX; i++) {
            if (data->buf[i] >= CLICK_MIN && data->buf[i] <= CLICK_MAX)
                valid_clicks++;
        }
        if (valid_clicks) {
            data->on_detect_cb(data->gpio, valid_clicks, data->cb_arg);
            if (ch_config.action_trigger_caps & click_actions[valid_clicks])
                supla_channel_emit_action(ch, click_actions[valid_clicks]);
        }
        memset(data->buf, 0, sizeof(data->buf));
        data->ev_num = 0;
        data->on_detect_cb(data->gpio, INPUT_EVENT_DONE, data->cb_arg);
        //supla_channel_emit_action(ch,SUPLA_ACTION_CAP_TURN_OFF);
    }
}

supla_channel_t *supla_generic_input_create(const struct generic_input_config *input_conf)
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
        .intr_type = GPIO_INTR_DISABLE //
    };

    esp_timer_create_args_t timer_args = {
        .name = "input",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = input_poll,
    };
    struct input_data *data;

    supla_channel_t *ch = supla_channel_create(&at_channel_config);
    if (!ch)
        return NULL;

    data = calloc(1, sizeof(struct input_data));
    if (!data) {
        supla_channel_free(ch);
        return NULL;
    }
    data->gpio = input_conf->gpio;
    data->level = 1;
    data->on_detect_cb = input_conf->on_event_cb;
    data->cb_arg = input_conf->arg;
    supla_channel_set_data(ch, data);

    gpio_config(&gpio_conf);
    timer_args.arg = ch;
    esp_timer_create(&timer_args, &data->timer);
    esp_timer_start_periodic(data->timer, DEFAULT_POLL_INTERVAL_US);
    return ch;
}
