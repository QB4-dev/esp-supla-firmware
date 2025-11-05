/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <status-led.h>
#include <relay-channel.h>

#define LED_SETTINGS_GR "LED"

static const char *TAG = "BSP";
static const char *led_state_labels[] = { [0] = "ON", [1] = "OFF", NULL };

static setting_t led_settings[] = {
    { .id = "ONLINE",
      .label = "ONLINE STATE",
      .type = SETTING_TYPE_ONEOF,
      .oneof = { 0, 0, led_state_labels } },
    {} //last element
};

static const settings_group_t board_settings[] = {
    { .id = LED_SETTINGS_GR, .label = "Led settings", .settings = led_settings },
    {}
};

static bsp_t brd_esp32_relay_ac_x1 = { .id = "ESP32-RELAY-AC-X1",
                                       .ver = "1.0",
                                       .settings_pack = board_settings };

bsp_t *const bsp = &brd_esp32_relay_ac_x1;

static supla_status_led_t *status_led;
static supla_channel_t    *relay_channel;

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

esp_err_t board_early_init(void)
{
    settings_nvs_read(bsp->settings_pack);
    return ESP_OK;
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    setting_t *led_setting = settings_pack_find(bsp->settings_pack, LED_SETTINGS_GR, "ONLINE");

    static button_t config_btn_conf = {
        .gpio = GPIO_NUM_0, .callback = button_cb //
    };
    struct status_led_config led_conf = {
        .gpio = GPIO_NUM_23,
        .online_set = led_setting ? led_setting->boolean.val : false //
    };
    struct relay_channel_config relay_channel_conf = {
        .gpio = GPIO_NUM_16,
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .supported_functions = RELAY_CH_SUPPORTED_FUNC_BITS
    };

    button_init(&config_btn_conf);
    status_led = supla_status_led_init(dev, &led_conf);
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
