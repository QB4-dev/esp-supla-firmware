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
#include <pca9632-rgbw-channel.h>

#ifdef CONFIG_BSP_ESP01_RGBW_v1_0

static const char *TAG = "BSP";

#define MODE_SETTINGS_GR "MODE"
#define COLOR_SETTINGS_GR "COLOR"

#define OUT_MODE_COLOR 0
#define OUT_MODE_4xDIMMER 1

static const char *mode_labels[] = {
    "COLOR", "4xDIMMER",
    NULL //last
};

static const char *map_labels[] = {
    [RGBW_MAP_RGBW] = "RGBW", [RGBW_MAP_GBRW] = "GBRW", [RGBW_MAP_GRBW] = "GRBW",
    [RGBW_MAP_GRWB] = "GRWB", [RGBW_MAP_BRGW] = "BRGW", NULL //last
};

static setting_t mode_settings[] = {
    { .id = "OUT", //
      .label = "OUTPUT",
      .type = SETTING_TYPE_ONEOF,
      .oneof = { OUT_MODE_COLOR, OUT_MODE_COLOR, mode_labels } },
    {} //last element
};

static setting_t color_settings[] = {
    { .id = "RGBW_MAP",
      .label = "COLOR MAP",
      .type = SETTING_TYPE_ONEOF,
      .oneof = { RGBW_MAP_GBRW, RGBW_MAP_GBRW, map_labels } },
    { .id = "RGB_ONLY",
      .label = "RGB ONLY",
      .type = SETTING_TYPE_BOOL,
      .boolean = { false, false } },
    { .id = "COLOR",
      .label = "MANUAL COLOR",
      .type = SETTING_TYPE_COLOR,
      .color = { { 0xFF, 0xFF, 0xFF } } },
    {} //last element
};

static const settings_group_t board_settings[] = {
    { .id = MODE_SETTINGS_GR, .label = "MODE", .settings = mode_settings },
    { .id = COLOR_SETTINGS_GR, .label = "COLOR", .settings = color_settings },
    {}
};

static bsp_t brd_esp01_rgbw = { .id = "ESP-01_RGBW",
                                .ver = "1.0",
                                .settings_pack = board_settings };

bsp_t *const bsp = &brd_esp01_rgbw;

static i2c_dev_t        pca9632;
static supla_channel_t *rgbw_channel;
static supla_channel_t *dimmer_channels[4];

static void button_cb(button_t *btn, button_state_t state)
{
    const uint8_t            brightness_steps[] = { 0, 25, 50, 75, 100 };
    static uint8_t           brightness_step = 0;
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value             *rgbw = (TRGBW_Value *)&new_value.value;
    EventBits_t              bits = device_get_event_bits();

    setting_t *output_mode = settings_pack_find(bsp->settings_pack, MODE_SETTINGS_GR, "OUT");
    setting_t *manual_color = settings_pack_find(bsp->settings_pack, COLOR_SETTINGS_GR, "COLOR");

    switch (state) {
    case BUTTON_CLICKED:
        ESP_LOGI(TAG, "btn clicked");
        switch (output_mode->oneof.val) {
        case OUT_MODE_COLOR:
            rgbw->colorBrightness = brightness_steps[++brightness_step % sizeof(brightness_steps)];
            rgbw->R = manual_color ? manual_color->color.r : 0xFF;
            rgbw->G = manual_color ? manual_color->color.g : 0xFF;
            rgbw->B = manual_color ? manual_color->color.b : 0xFF;
            pca9632_channel_set_value(rgbw_channel, &new_value);
            break;
        case OUT_MODE_4xDIMMER:
            rgbw->brightness = brightness_steps[++brightness_step % sizeof(brightness_steps)];
            for (int led = LED0; led <= LED3; led++)
                pca9632_channel_set_value(dimmer_channels[led], &new_value);
            break;
        default:
            break;
        }
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
    button_init(&btn);

    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(
        pca9632_init_desc(&pca9632, PCA9632_I2C_ADDR, I2C_NUM_0, GPIO_NUM_2, GPIO_NUM_3));

    ESP_ERROR_CHECK(pca9632_init(&pca9632));
    ESP_ERROR_CHECK(pca9632_set_output_params(&pca9632, true, OUTDRV_TOTEM_POLE));
    return ESP_OK;
}

static void pulse_task(void *arg)
{
    setting_t *output_mode = settings_pack_find(bsp->settings_pack, MODE_SETTINGS_GR, "OUT");
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value             *rgbw = (TRGBW_Value *)&new_value.value;
    EventBits_t              bits;

    if (!output_mode) {
        ESP_LOGE(TAG, "output_mode setting not found");
        return;
    }

    while (1) {
        bits = device_get_event_bits();
        if (bits & DEVICE_CONFIG_EVENT_BIT) {
            rgbw->R = rgbw->R ? 0 : 255;
            rgbw->G = rgbw->G ? 0 : 255;
            rgbw->B = rgbw->B ? 0 : 255;
            rgbw->colorBrightness = rgbw->colorBrightness ? 0 : 100;
            rgbw->brightness = rgbw->brightness ? 0 : 100;

            switch (output_mode->oneof.val) {
            case OUT_MODE_COLOR:
                pca9632_channel_set_value(rgbw_channel, &new_value);
                break;
            case OUT_MODE_4xDIMMER:
                for (int led = LED0; led <= LED3; led++)
                    pca9632_channel_set_value(dimmer_channels[led], &new_value);
                break;
            default:
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    setting_t *output_mode = settings_pack_find(bsp->settings_pack, MODE_SETTINGS_GR, "OUT");
    setting_t *rgbw_map = settings_pack_find(bsp->settings_pack, COLOR_SETTINGS_GR, "RGBW_MAP");
    setting_t *rgb_only = settings_pack_find(bsp->settings_pack, COLOR_SETTINGS_GR, "RGB_ONLY");

    if (!output_mode) {
        ESP_LOGE(TAG, "output_mode setting not found");
        return ESP_FAIL;
    }

    switch (output_mode->oneof.val) {
    case OUT_MODE_COLOR: {
        struct pca9632_rgbw_channel_config rgbw_channel_conf = {
            .pca9632 = &pca9632,                                        //set chip
            .rgbw_map = rgbw_map ? rgbw_map->oneof.val : RGBW_MAP_GBRW, //set default pwm order
            .rgb_only = rgb_only ? rgb_only->boolean.val : false        //use white channel
        };

        rgbw_channel = pca9632_rgbw_channel_create(&rgbw_channel_conf);
        supla_dev_add_channel(dev, rgbw_channel);
        ESP_LOGI(TAG, "RGBW channel created");
    } break;
    case OUT_MODE_4xDIMMER: {
        struct pca9632_dimmer_channel_config dimmer_channel_conf = {
            .pca9632 = &pca9632, //set chip
        };

        for (int led = LED0; led <= LED3; led++) {
            dimmer_channel_conf.pwm_output = led;
            dimmer_channels[led] = pca9632_dimmer_channel_create(&dimmer_channel_conf);
            supla_dev_add_channel(dev, dimmer_channels[led]);
        }
        ESP_LOGI(TAG, "4xDIMMER channel created");
    } break;
    default:
        break;
    }

    xTaskCreate(&pulse_task, "pulse", 2048, NULL, tskIDLE_PRIORITY, NULL);
    ESP_LOGI(TAG, "board init completed OK");
    return ESP_OK;
}

esp_err_t board_on_config_mode_init(void)
{
    TSD_SuplaChannelNewValue new_value = {};
    pca9632_channel_set_value(rgbw_channel, &new_value);
    return ESP_OK;
}

esp_err_t board_on_config_mode_exit(void)
{
    TSD_SuplaChannelNewValue new_value = {};
    pca9632_channel_set_value(rgbw_channel, &new_value);
    return ESP_OK;
}

#endif
