/*
 * esp-4ch.c
 *
 *  Created on: 29 sie 2023
 *      Author: kuba
 */

#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <generic-input.h>
#include <relay-channel.h>

#ifdef CONFIG_BSP_ESP12F_RELAY_X4_v1_2

static const char *TAG = "BSP";

static const settings_group_t board_settings[] = { {} };

static bsp_t brd_esp12f_relay_x4_v1_2 = { .id = "ESP12F_RELAY_X4",
                                          .ver = "1.2",
                                          .settings_pack = board_settings };

bsp_t *const bsp = &brd_esp12f_relay_x4_v1_2;

static supla_dev_t     *supla_dev;
static supla_channel_t *relay1_channel;
static supla_channel_t *relay2_channel;
static supla_channel_t *relay3_channel;
static supla_channel_t *relay4_channel;

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

static button_t btn = {
    .gpio = GPIO_NUM_4,
    .internal_pull = true,
    .callback = button_cb //
};

esp_err_t board_early_init(void)
{
    button_init(&btn);
    return ESP_OK;
}

esp_err_t board_init(supla_dev_t *dev)
{
    struct relay_channel_config relay1_channel_conf = {
        .gpio = GPIO_NUM_16,
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .supported_functions = 0xff,
    };

    struct relay_channel_config relay2_channel_conf = {
        .gpio = GPIO_NUM_14,
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .supported_functions = 0xff,
    };

    struct relay_channel_config relay3_channel_conf = {
        .gpio = GPIO_NUM_12,
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .supported_functions = 0xff,
    };

    struct relay_channel_config relay4_channel_conf = {
        .gpio = GPIO_NUM_13,
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .supported_functions = 0xff,
    };

    relay1_channel = supla_relay_channel_create(&relay1_channel_conf);
    relay2_channel = supla_relay_channel_create(&relay2_channel_conf);
    relay3_channel = supla_relay_channel_create(&relay3_channel_conf);
    relay4_channel = supla_relay_channel_create(&relay4_channel_conf);

    supla_dev_add_channel(dev, relay1_channel);
    supla_dev_add_channel(dev, relay2_channel);
    supla_dev_add_channel(dev, relay3_channel);
    supla_dev_add_channel(dev, relay4_channel);

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
