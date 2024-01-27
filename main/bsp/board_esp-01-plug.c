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

#include <ledc-channel.h>
#include <dht-sensor.h>
#include <button.h>

#ifdef CONFIG_BSP_ESP01_PLUG_DHT

static const char *TAG="BSP";

static bsp_t brd_esp01_dht =
{
    .id = "ESP-01 DHT"
};

bsp_t * const bsp = &brd_esp01_dht;

static supla_channel_t *dht_channel;
static EventGroupHandle_t bsp_event_group;

#define CONFIG_MODE_BIT BIT0

static esp_err_t board_config_mode_init(void)
{
    xEventGroupSetBits(bsp_event_group, CONFIG_MODE_BIT);
    device_init_config();
    return ESP_OK;
}

static esp_err_t board_config_mode_exit(void)
{
    xEventGroupClearBits(bsp_event_group, CONFIG_MODE_BIT);
    device_exit_config();
    return ESP_OK;
}

static void button_cb(button_t *btn, button_state_t state)
{
    EventBits_t bits = xEventGroupWaitBits(bsp_event_group,CONFIG_MODE_BIT,0,0,1);

    switch (state) {
        case BUTTON_CLICKED:
            ESP_LOGI(TAG, "btn clicked");
            break;
        case BUTTON_PRESSED_LONG:
            ESP_LOGI(TAG, "btn pressed long");
            if(!(bits & CONFIG_MODE_BIT))
                board_config_mode_init();
            else
                board_config_mode_exit();
            break;
        default:
            break;
    }
}

static button_t btn = {
    .gpio = GPIO_NUM_0,
    .callback = button_cb
};

esp_err_t board_init(supla_dev_t *dev)
{
    bsp_event_group = xEventGroupCreate();
    button_init(&btn);
    dht_channel = supla_channel_dht_create(DHT_TYPE_DHT11,GPIO_NUM_2,1000);

    supla_dev_set_name(dev,bsp->id);
    supla_dev_add_channel(dev,dht_channel);

    ESP_LOGI(TAG,"board init completed OK");
    return ESP_OK;
}
#endif
