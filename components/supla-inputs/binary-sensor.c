#include <binary-sensor.h>
#include <stdlib.h>
#include <string.h>
#include <esp_timer.h>
#include <esp-supla.h>

#define DEFAULT_POLL_INTERVAL_US 1000000 //100ms

struct sensor_nvs_config {
    int active_func;
    union {
        TChannelConfig_BinarySensor binary_sensor;
    };
};

struct sensor_data {
    gpio_num_t               gpio;
    esp_timer_handle_t       timer;
    struct sensor_nvs_config nvs_config;
};

static int supla_binary_sensor_channel_init(supla_channel_t *ch)
{
    struct sensor_data *data = supla_channel_get_data(ch);

    supla_esp_nvs_channel_config_restore(ch, &data->nvs_config, sizeof(data->nvs_config));

    supla_log(LOG_INFO, "bin_sensor:%d func=%d", supla_channel_get_assigned_number(ch),
              data->nvs_config.active_func);
    return SUPLA_RESULTCODE_TRUE;
}

static int supla_srv_binary_sensor_config(supla_channel_t *ch, TSD_ChannelConfig *config)
{
    TChannelConfig_BinarySensor *sensor_conf;
    struct sensor_data          *data = supla_channel_get_data(ch);

    if (config->ConfigType == SUPLA_CONFIG_TYPE_DEFAULT && config->ConfigSize > 0) {
        sensor_conf = (TChannelConfig_BinarySensor *)config->Config;

        supla_log(LOG_INFO, "got binary sensor config: inv=%d filter=%dms",
                  sensor_conf->InvertedLogic, sensor_conf->FilteringTimeMs);

        data->nvs_config.active_func = config->Func;
        data->nvs_config.binary_sensor = *sensor_conf;
        supla_esp_nvs_channel_config_store(ch, &data->nvs_config, sizeof(data->nvs_config));

        if (sensor_conf->FilteringTimeMs) {
            esp_timer_stop(data->timer);
            esp_timer_start_periodic(data->timer, sensor_conf->FilteringTimeMs * 1000);
        }
    }
    return ESP_OK;
}

static void input_poll(void *arg)
{
    supla_channel_t    *ch = arg;
    struct sensor_data *data = supla_channel_get_data(ch);
    const int           level = gpio_get_level(data->gpio);
    const int           inv_logic = data->nvs_config.binary_sensor.InvertedLogic;

    supla_channel_set_binary_value(ch, inv_logic ? !level : level);
}

supla_channel_t *supla_binary_sensor_create(const struct binary_sensor_config *config)
{
    supla_channel_config_t sensor_channel_config = {
        .type = SUPLA_CHANNELTYPE_BINARYSENSOR,
        .supported_functions = config->supported_functions,
        .default_function = config->default_function,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
        .sync_values_onchange = 1,
        .on_channel_init = supla_binary_sensor_channel_init,
        .on_config_recv = supla_srv_binary_sensor_config //
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
    esp_timer_start_periodic(data->timer, DEFAULT_POLL_INTERVAL_US);
    return ch;
}

int supla_binary_sensor_delete(supla_channel_t *ch)
{
    struct sensor_data *data = supla_channel_get_data(ch);
    esp_timer_delete(data->timer);
    free(data);
    return supla_channel_free(ch);
}

int supla_binary_sensor_get_local(supla_channel_t *ch)
{
    struct sensor_data *data = supla_channel_get_data(ch);
    const int           level = gpio_get_level(data->gpio);
    const int           inv_logic = data->nvs_config.binary_sensor.InvertedLogic;

    return inv_logic ? !level : level;
}
