/*
 * esp-4ch.c
 *
 *  Created on: 29 sie 2023
 *      Author: kuba
 */

#include <click-input.h>

#include "board.h"
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <button.h>
#include <rgbw-channel.h>

#ifdef CONFIG_BSP_ESP01_RGBW

static const char *TAG = "BSP";

static bsp_t brd_esp01_rgbw = { .id = "ESP-01 RGBW" };

bsp_t *const bsp = &brd_esp01_rgbw;

static supla_channel_t *rgbw_channel;

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

esp_err_t board_init(supla_dev_t *dev)
{
    struct rgbw_channel_config rgbw_channel_config = { .fade_time = 10 };

    rgbw_channel = supla_rgbw_channel_create(&rgbw_channel_config);

    button_init(&btn);

    supla_dev_set_name(dev, bsp->id);
    supla_dev_add_channel(dev, rgbw_channel);

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
