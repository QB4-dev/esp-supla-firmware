#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <status-led-rgb.h>
#include <relay-channel.h>
#include <link-thermometer.h>

static const char *TAG = "BSP";

#define LINK_SETTINGS_GR "THERM"

static char therm1_link[256];

static setting_t therm_settings[] = {
    { .id = "TH1",
      .label = "Thermometer 1 link",
      .type = SETTING_TYPE_TEXT,
      .text = { therm1_link, "", sizeof(therm1_link) } },
    {} //last element
};

static const settings_group_t board_settings_pack[] = {
    { .id = LINK_SETTINGS_GR, .label = "Links", .settings = therm_settings },
    {} //last element
};

static bsp_t brd_esp32s3_relay_6ch = { .id = "ESP32-S3-RELAY-6CH",
                                       .ver = "1.0",
                                       .settings_pack = board_settings_pack };

bsp_t *const bsp = &brd_esp32s3_relay_6ch;

static supla_status_led_rgb_t status_led;
static supla_channel_t       *link_thermometer;
static supla_channel_t       *relay_channels[6];

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
    ESP_ERROR_CHECK(button_init(&btn));
    settings_nvs_read(bsp->settings_pack);
    settings_pack_print(bsp->settings_pack);
    return ESP_OK;
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    setting_t *th1_link_setting = settings_pack_find(bsp->settings_pack, LINK_SETTINGS_GR, "TH1");

    const gpio_num_t relay_gpio[6] = { GPIO_NUM_1,  GPIO_NUM_2,  GPIO_NUM_41,
                                       GPIO_NUM_42, GPIO_NUM_45, GPIO_NUM_46 };

    struct relay_channel_config relay_channel_conf = {
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .supported_functions = RELAY_CH_SUPPORTED_FUNC_BITS,
    };
    struct status_led_rgb_config led_conf = {
        .gpio = GPIO_NUM_38,
        .type = LED_MODEL_WS2812,
        .pixel_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
        .color = { .g = 0xff },
    };

    status_led = supla_status_led_rgb_init(dev, &led_conf);
    if (!status_led)
        return ESP_FAIL;

    struct link_thermometer_config link_thermometer_conf = {
        .default_caption = "LINK-THERM",
        .link = th1_link_setting ? th1_link_setting->text.val : NULL,
        .poll_interval_ms = 10000,
    };
    link_thermometer = link_thermometer_create(&link_thermometer_conf);
    if (!link_thermometer) {
        ESP_LOGE(TAG, "Failed to create link thermometer channel");
        return ESP_ERR_NO_MEM;
    }
    if (supla_dev_add_channel(dev, link_thermometer) != SUPLA_RESULT_TRUE) {
        ESP_LOGE(TAG, "Failed to add link thermometer channel");
        link_thermometer_delete(link_thermometer);
        link_thermometer = NULL;
        return ESP_FAIL;
    }

    for (int i = 0; i < sizeof(relay_gpio) / sizeof(relay_gpio[0]); i++) {
        relay_channel_conf.gpio = relay_gpio[i];
        relay_channels[i] = supla_relay_channel_create(&relay_channel_conf);
        if (!relay_channels[i]) {
            ESP_LOGE(TAG, "Failed to create relay channel %d", i);
            return ESP_ERR_NO_MEM;
        }
        if (supla_dev_add_channel(dev, relay_channels[i]) != SUPLA_RESULT_TRUE) {
            ESP_LOGE(TAG, "Failed to add relay channel %d", i);
            return ESP_FAIL;
        }
    }

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
