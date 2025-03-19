#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <relay-channel.h>

static const char *TAG = "BSP";

static const settings_group_t board_settings_pack[] = { {} };

static bsp_t brd_esp32s3_relay_6ch = { .id = "ESP32-S3-RELAY-6CH",
                                       .ver = "1.0",
                                       .settings_pack = board_settings_pack };

bsp_t *const bsp = &brd_esp32s3_relay_6ch;

static supla_dev_t     *supla_dev;
static supla_channel_t *relay_channels[6];

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
    button_init(&btn);
    return ESP_OK;
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    const gpio_num_t relay_gpio[6] = { GPIO_NUM_1,  GPIO_NUM_2,  GPIO_NUM_41,
                                       GPIO_NUM_42, GPIO_NUM_45, GPIO_NUM_46 };

    struct relay_channel_config relay_channel_conf = {
        .gpio = GPIO_NUM_16,
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .supported_functions = RELAY_CH_SUPPORTED_FUNC_BITS,
    };

    for (int i = 0; i < sizeof(relay_gpio) / sizeof(relay_gpio[0]); i++) {
        relay_channel_conf.gpio = relay_gpio[i];
        relay_channels[i] = supla_relay_channel_create(&relay_channel_conf);
        supla_dev_add_channel(dev, relay_channels[i]);
    }

    supla_dev = dev; //store pointer
    ESP_LOGI(TAG, "board init completed OK");
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
