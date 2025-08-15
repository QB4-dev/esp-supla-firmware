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
#include "rs-mp46-channel.h"

#define MOTOR_SETTINGS_GR "MOTOR"

#ifdef CONFIG_BSP_ESP32

static const char *TAG = "BSP";

static setting_t motor_settings[] = {
    { .id = "INV", .label = "INVERTED", .type = SETTING_TYPE_BOOL, .boolean = { true, true } },
    { .id = "PTS", .label = "PULL TO START", .type = SETTING_TYPE_BOOL, .boolean = { true, true } },
    {} //last element
};

static const settings_group_t board_settings_pack[] = {
    { .id = MOTOR_SETTINGS_GR, .label = "MOTOR", .settings = motor_settings },
    {}
};

static bsp_t brd_nodemcu = { .id = "ESP32-TUYA-RS",
                             .ver = "1.0",
                             .settings_pack = board_settings_pack };

bsp_t *const bsp = &brd_nodemcu;

static supla_channel_t *rs_channel;

static esp_err_t settings_handler(const settings_group_t *settings, void *arg)
{
    supla_channel_t *ch = arg;

    setting_t *inv = settings_pack_find(bsp->settings_pack, MOTOR_SETTINGS_GR, "INV");
    setting_t *pts = settings_pack_find(bsp->settings_pack, MOTOR_SETTINGS_GR, "PTS");

    supla_mp46_rs_channel_set_direction(ch, inv ? inv->boolean.val : false);
    supla_mp46_rs_channel_set_pull_to_start(ch, pts ? pts->boolean.val : true);
    return ESP_OK;
}

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
    settings_nvs_read(bsp->settings_pack);
    settings_pack_print(bsp->settings_pack);
    return ESP_OK;
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    struct mp46_rs_channel_config rs_channel_conf = {
        .uart = TUYA_MCU_CONFIG_DEFAULT(),
    };

    button_init(&btn);
    rs_channel = supla_mp46_rs_channel_create(&rs_channel_conf);
    supla_dev_add_channel(dev, rs_channel);

    settings_handler_register(settings_handler, rs_channel);
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
