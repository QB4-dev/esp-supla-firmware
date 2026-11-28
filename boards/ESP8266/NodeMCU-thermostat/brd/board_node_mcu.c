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
#include <link-thermometer.h>
#include <thermostat-channel.h>

#ifdef CONFIG_BSP_NODE_MCU

static const char *TAG = "BSP";

#define COLOR_SETTINGS_GR "GR"

static const settings_group_t board_settings[] = { {} };

static bsp_t brd_nodemcu = { .id = "NodeMCU", .ver = "1.0", .settings_pack = board_settings };

bsp_t *const bsp = &brd_nodemcu;

static supla_dev_t        *supla_dev;
static supla_status_led_t *status_led;
static supla_channel_t    *hvac_channel;

static supla_channel_t *temp_channel;

static supla_channel_config_t temp_channel_config = {
    .type = SUPLA_CHANNELTYPE_THERMOMETER,
    .supported_functions = SUPLA_CHANNELFNC_THERMOMETER,
    .default_function = SUPLA_CHANNELFNC_THERMOMETER,
};

static int read_temperature_c(void *context, float *temperature_c)
{
    // Read the temperature from the sensor
    // For now, return a dummy value
    *temperature_c = 25.0;
    return 0;
}
static int set_heating(void *context, bool enabled)
{
    // Set the heating state
    // For now, just log the state
    ESP_LOGI(TAG, "Heating %s", enabled ? "enabled" : "disabled");
    return 0;
}

static struct thermostat_channel_hal thermostat_hal = {
    .read_temperature_c = read_temperature_c,
    .set_heating = set_heating,
};

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
    esp_set_cpu_freq(ESP_CPU_FREQ_160M);
    settings_nvs_read(bsp->settings_pack);
    settings_pack_print(bsp->settings_pack);
    button_init(&btn);
    return ESP_OK;
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    struct status_led_config led_conf = {
        .gpio = GPIO_NUM_2,
        .online_set = true //
    };

    temp_channel = supla_channel_create(&temp_channel_config);

    struct thermostat_channel_config hvac_channel_conf = {
        .hal = thermostat_hal,
        .thermometer_channel = temp_channel,
        .setpoint_c = 22.0,      // default setpoint in Celsius
        .hysteresis_c = 0.5,     // default hysteresis in Celsius
        .poll_interval_ms = 1000 // default poll interval in milliseconds
    };

    status_led = supla_status_led_init(dev, &led_conf);
    hvac_channel = supla_thermostat_channel_create(&hvac_channel_conf);

    supla_dev_add_channel(dev, temp_channel);
    supla_dev_add_channel(dev, hvac_channel);
    //supla_dev_enable_notifications(dev, 0x00);

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

#endif
