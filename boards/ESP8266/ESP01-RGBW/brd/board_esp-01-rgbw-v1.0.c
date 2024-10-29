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
#include <ext-rgbw-channel.h>

#ifdef CONFIG_BSP_ESP01_RGBW_v1_0

static const char *TAG = "BSP";

static const settings_group_t board_settings[] = { {} };

static bsp_t brd_esp01_dimmer = { .id = "ESP-01_RGBW",
                                  .ver = "1.0",
                                  .settings_pack = board_settings };

bsp_t *const bsp = &brd_esp01_dimmer;

static i2c_dev_t        pca9632;
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

esp_err_t board_supla_init(supla_dev_t *dev)
{
    settings_nvs_read(bsp->settings_pack);
    button_init(&btn);

    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(
        pca9632_init_desc(&pca9632, PCA9632_I2C_ADDR, I2C_NUM_0, GPIO_NUM_2, GPIO_NUM_3));

    ESP_ERROR_CHECK(pca9632_init(&pca9632));
    ESP_ERROR_CHECK(pca9632_set_output_params(&pca9632, true, true));

    struct ext_rgbw_channel_config rgbw_channel_conf = {
        .pca9632 = &pca9632 //
    };

    rgbw_channel = supla_ext_rgbw_channel_create(&rgbw_channel_conf);

    supla_dev_add_channel(dev, rgbw_channel);
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
