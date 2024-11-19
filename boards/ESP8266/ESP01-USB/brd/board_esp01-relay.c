#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <generic-input.h>
#include <dht-sensor.h>
#include <relay-channel.h>

#ifdef CONFIG_BSP_ESP01_USB

static const char *TAG = "BSP";

static const settings_group_t board_settings[] = { {} };

static bsp_t brd_esp01_usb = { .id = "ESP-01 USB RELAY",
                               .ver = "1.0",
                               .settings_pack = board_settings };

bsp_t *const bsp = &brd_esp01_usb;

static supla_channel_t *relay_channel;

static void button_cb(button_t *btn, button_state_t state)
{
    TRelayChannel_Value relay_value = {};
    EventBits_t         bits = device_get_event_bits();
    switch (state) {
    case BUTTON_CLICKED:
        ESP_LOGI(TAG, "btn clicked");
        relay_value.hi = !supla_relay_channel_get_state(relay_channel);
        supla_relay_channel_set_local(relay_channel, &relay_value);
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
    struct relay_channel_config relay_channel_conf = {
        .gpio = GPIO_NUM_2,
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .supported_functions = RELAY_CH_SUPPORTED_FUNC_BITS
    };
    settings_nvs_read(bsp->settings_pack);
    button_init(&btn);
    relay_channel = supla_relay_channel_create(&relay_channel_conf);

    supla_dev_add_channel(dev, relay_channel);
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
