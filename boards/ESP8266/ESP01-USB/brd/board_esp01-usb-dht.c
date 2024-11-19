#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <generic-input.h>
#include <dht-sensor.h>
#include <relay-channel.h>

#ifdef CONFIG_BSP_ESP01_USB_DHT

static const char *TAG = "BSP";

static const char *dht_type_labels[] = { [DHT_TYPE_DHT11] = "DHT11",
                                         [DHT_TYPE_AM2301] = "AM2301",
                                         [DHT_TYPE_SI7021] = "SI7021",
                                         NULL };

static setting_t sensor_settings[] = {
    { .id = "TYPE",
      .label = "SENSOR_TYPE",
      .type = SETTING_TYPE_ONEOF,
      .oneof = { DHT_TYPE_DHT11, DHT_TYPE_DHT11, dht_type_labels } },
    {} //last element
};

static const settings_group_t board_settings[] = {
    { .id = "DHT", .label = "TEMP/HUMID SENSOR", .settings = sensor_settings },
    {}
};

static bsp_t brd_esp01_usb = { .id = "ESP-01 USB DHT",
                               .ver = "1.0",
                               .settings_pack = board_settings };

bsp_t *const bsp = &brd_esp01_usb;

static supla_channel_t *dht_channel;

static void button_cb(button_t *btn, button_state_t state)
{
    EventBits_t bits = device_get_event_bits();
    switch (state) {
    case BUTTON_CLICKED:
        ESP_LOGI(TAG, "btn clicked");
        break;
    case BUTTON_PRESSED_LONG:
        ESP_LOGI(TAG, "btn pressed long");
        if (!(bits & DEVICE_CONFIG_EVENT_BIT))
            device_init_config();
        else
            device_exit_config();
        break;
    default:
        break;
    }
}

static button_t btn = { .gpio = GPIO_NUM_0, .callback = button_cb };

esp_err_t board_early_init(void)
{
    return ESP_OK;
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    settings_nvs_read(bsp->settings_pack);
    setting_t *dht_type_set = settings_pack_find(bsp->settings_pack, "DHT", "TYPE");

    struct dht_channel_config dht_conf = {
        .sensor_type = dht_type_set ? dht_type_set->oneof.val : DHT_TYPE_AM2301,
        .gpio = GPIO_NUM_2,
    };

    button_init(&btn);
    dht_channel = supla_channel_dht_create(&dht_conf);
    supla_dev_add_channel(dev, dht_channel);

    ESP_LOGI(TAG, "board init completed with sensor: %s", dht_type_labels[dht_conf.sensor_type]);
    return ESP_OK;
}

esp_err_t board_on_config_mode_init(void)
{
    return ESP_OK;
}

esp_err_t board_on_config_mode_exit(void)
{
    return ESP_OK;
}

#endif
