/*
 * esp-4ch.c
 *
 *  Created on: 29 sie 2023
 *      Author: kuba
 */

#include "board.h"
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <button.h>
#include <generic-input.h>
#include <dht-sensor.h>
#include <relay-channel.h>

#ifdef CONFIG_BSP_ESP01_USB

static const char *TAG = "BSP";

static const settings_group_t board_settings_pack[] = { {} };

static bsp_t brd_esp01_usb = { .id = "ESP-01 USB", .settings_pack = board_settings_pack };

bsp_t *const bsp = &brd_esp01_usb;

static supla_channel_t *dht_channel;
static supla_channel_t *input_channel;
static supla_channel_t *relay_channel;
static supla_dev_t     *supla_dev;

static void button_cb(button_t *btn, button_state_t state)
{
    EventBits_t bits = device_get_event_bits();
    switch (state) {
    case BUTTON_CLICKED:
        ESP_LOGI(TAG, "btn clicked");
        supla_dev_send_notification(supla_dev, -1, "ESP-01", "PUSH notification", 0);
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
    struct relay_channel_config relay_channel_conf = {
        .gpio = GPIO_NUM_2,
        .default_function = SUPLA_CHANNELFNC_STAIRCASETIMER,
        .supported_functions = 0xff
        //SUPLA_BIT_FUNC_POWERSWITCH | SUPLA_BIT_FUNC_LIGHTSWITCH | SUPLA_BIT_FUNC_STAIRCASETIMER
    };

    button_init(&btn);
    //dht_channel = supla_channel_dht_create(DHT_TYPE_DHT11,GPIO_NUM_2,1000);
    relay_channel = supla_relay_channel_create(&relay_channel_conf);

    supla_dev_add_channel(dev, relay_channel);

    supla_dev_enable_notifications(dev, 0x00);

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
