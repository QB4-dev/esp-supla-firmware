#include <binary-sensor.h>
#include <stdlib.h>
#include <string.h>
#include <esp_timer.h>
#include <esp_log.h>

#define POLL_INTERVAL_US 1000000 //100ms

struct sensor_data {
    gpio_num_t         gpio;
    bool               inv_logic;
    esp_timer_handle_t timer;
};

static int supla_sensor_config(supla_channel_t *ch, TSD_ChannelConfig *config)
{
    struct sensor_data *data = supla_channel_get_data(ch);

    if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT) {
        TChannelConfig_BinarySensor *sensor_conf = (TChannelConfig_BinarySensor *)config->Config;
        data->inv_logic = sensor_conf->InvertedLogic;
        supla_log(LOG_INFO, "got binary sensor config: inv=%d", sensor_conf->InvertedLogic);
    }
    return ESP_OK;
}

static void input_poll(void *arg)
{
    supla_channel_t       *ch = arg;
    supla_channel_config_t ch_config;
    struct sensor_data    *data;
    int                    level;

    data = supla_channel_get_data(ch);
    supla_channel_get_config(ch, &ch_config);

    level = gpio_get_level(data->gpio);
    supla_channel_set_binary_value(ch, data->inv_logic ? !level : level);
}

supla_channel_t *supla_binary_sensor_create(const struct binary_sensor_config *config)
{
    supla_channel_config_t sensor_channel_config = {
        .type = SUPLA_CHANNELTYPE_BINARYSENSOR,
        .supported_functions = config->supported_functions,
        .default_function = config->default_function,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
        .sync_values_onchange = 1,
        .on_config_recv = supla_sensor_config //
    };

    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1 << config->gpio),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_DISABLE //
    };

    esp_timer_create_args_t timer_args = {
        .name = "binary-sensor",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = input_poll,
    };
    struct sensor_data *data;

    supla_channel_t *ch = supla_channel_create(&sensor_channel_config);
    if (!ch)
        return NULL;

    data = calloc(1, sizeof(struct sensor_data));
    if (!data) {
        supla_channel_free(ch);
        return NULL;
    }
    data->gpio = config->gpio;
    supla_channel_set_data(ch, data);

    gpio_config(&gpio_conf);
    timer_args.arg = ch;
    esp_timer_create(&timer_args, &data->timer);
    esp_timer_start_periodic(data->timer, POLL_INTERVAL_US);
    return ch;
}
