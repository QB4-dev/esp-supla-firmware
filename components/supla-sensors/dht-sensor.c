#include "include/dht-sensor.h"

#include <stdlib.h>
#include <esp_timer.h>

struct dht_channel_data {
    dht_sensor_type_t  sensor_type;
    gpio_num_t         gpio;
    esp_timer_handle_t timer;
};

static void dht_poll(void *arg)
{
    supla_channel_t         *ch = arg;
    struct dht_channel_data *dht_data;
    float                    humid, temp;
    esp_err_t                rc;

    dht_data = supla_channel_get_data(ch);
    rc = dht_read_float_data(dht_data->sensor_type, dht_data->gpio, &humid, &temp);
    if (rc == ESP_OK) {
        supla_channel_set_humidtemp_value(ch, humid, temp);
    }
}

supla_channel_t *supla_channel_dht_create(const struct dht_channel_config *config)
{
    const supla_channel_config_t temphumid_channel_config = {
        .type = SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR,
        .supported_functions = SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE,
        .default_function = SUPLA_CHANNELFNC_HUMIDITYANDTEMPERATURE,
        .default_caption = "DHT",
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE
    };
    uint32_t                 poll_interval;
    struct dht_channel_data *dht_data;
    esp_timer_create_args_t  timer_args = {
         .name = "dht",
         .dispatch_method = ESP_TIMER_TASK,
         .callback = dht_poll,
    };

    supla_channel_t *ch = supla_channel_create(&temphumid_channel_config);
    if (!ch)
        return NULL;

    dht_data = malloc(sizeof(struct dht_channel_data));
    if (!dht_data) {
        supla_channel_free(ch);
        return NULL;
    }

    dht_data->sensor_type = config->sensor_type;
    dht_data->gpio = config->gpio;
    poll_interval = config->poll_interval_ms ? config->poll_interval_ms : 10000;

    supla_channel_set_data(ch, dht_data);
    timer_args.arg = ch;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &dht_data->timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(dht_data->timer, poll_interval * 1000));
    return ch;
}
