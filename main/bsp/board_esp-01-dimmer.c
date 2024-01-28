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

#include <ledc-channel.h>

#ifdef CONFIG_BSP_ESP01_DIMMER

static const char *TAG="BSP";

static bsp_t brd_esp01_dimmer =
{
    .id = "ESP-01 Dimmer"
};

bsp_t * const bsp = &brd_esp01_dimmer;

static supla_channel_t *ledc_channel;
static supla_channel_t *input1_channel;
static supla_channel_t *input2_channel;

static void detect_cb(input_event_t event, void *arg)
{
    const char *events[] = {
        [INPUT_EVENT_INIT] = "INPUT_EVENT_INIT",
        [INPUT_EVENT_CLICKx1] = "INPUT_EVENT_CLICKx1",
        [INPUT_EVENT_CLICKx2] = "INPUT_EVENT_CLICKx2",
        [INPUT_EVENT_CLICKx3] = "INPUT_EVENT_CLICKx3",
        [INPUT_EVENT_CLICKx4] = "INPUT_EVENT_CLICKx4",
        [INPUT_EVENT_CLICKx5] = "INPUT_EVENT_CLICKx5",
        [INPUT_EVENT_DONE] = "INPUT_EVENT_DONE"
    };
    supla_channel_t *ch = arg;
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value *rgbw = (TRGBW_Value*)&new_value.value;
    EventBits_t bits = device_get_event_bits();

    ESP_LOGI(TAG, "detect_cb event = %s", events[event]);

    switch (event) {
        case INPUT_EVENT_INIT:
            if(!(bits & DEVICE_CONFIG_EVENT_BIT)){
                rgbw->brightness = 100;
                supla_ledc_channel_set_brightness(ch,&new_value);
            }
            break;
        case INPUT_EVENT_CLICKx3:
            if(!(bits & DEVICE_CONFIG_EVENT_BIT))
                device_init_config();
            else
                device_exit_config();
            break;
        case INPUT_EVENT_DONE:
            if(!(bits & DEVICE_CONFIG_EVENT_BIT)){
                new_value.DurationMS = 5000;
                rgbw->brightness = 100;
                supla_ledc_channel_set_brightness(ch,&new_value);
            }
            break;
        default:
            break;
    }
}

static void pulse_task(void *arg)
{
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value *rgbw = (TRGBW_Value*)&new_value.value;
    EventBits_t bits;

    while(1){
        bits = device_get_event_bits();
        if(bits & DEVICE_CONFIG_EVENT_BIT){
            rgbw->brightness = rgbw->brightness ? 0 : 100;
            supla_ledc_channel_set_brightness(ledc_channel,&new_value);
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

esp_err_t board_early_init(void)
{
    return ESP_OK;
}

esp_err_t board_init(supla_dev_t *dev)
{
    struct ledc_channel_config ledc_channel_conf = {
            .gpio = GPIO_NUM_3,
            .ledc_channel = LEDC_CHANNEL_0,
            .fade_time = 1000
    };
    ledc_channel = supla_ledc_channel_create(&ledc_channel_conf);

    struct click_input_config pir1_conf = {
        .gpio = GPIO_NUM_0,
        .action_trigger_caps =
            SUPLA_ACTION_CAP_TURN_ON |
            SUPLA_ACTION_CAP_TURN_OFF,
        .on_detect_cb = detect_cb,
        .arg = ledc_channel
    };
    struct click_input_config pir2_conf = {
        .gpio = GPIO_NUM_2,
        .action_trigger_caps =
            SUPLA_ACTION_CAP_TURN_ON |
            SUPLA_ACTION_CAP_TURN_OFF,
        .on_detect_cb = detect_cb,
        .arg = ledc_channel
    };

    input1_channel = supla_click_input_create(&pir1_conf);
    input2_channel = supla_click_input_create(&pir2_conf);

    supla_dev_set_name(dev,bsp->id);
    supla_dev_add_channel(dev,ledc_channel);
    supla_dev_add_channel(dev,input1_channel);
    supla_dev_add_channel(dev,input2_channel);

    xTaskCreate(&pulse_task, "pulse", 2048, NULL, 1, NULL);
    ESP_LOGI(TAG,"board init completed OK");
    return ESP_OK;
}

esp_err_t board_on_config_mode_init(void)
{
    return ESP_OK;
}

esp_err_t board_on_config_mode_exit(void)
{
    TSD_SuplaChannelNewValue new_value = {};

    supla_ledc_channel_set_brightness(ledc_channel,&new_value);
    return ESP_OK;
}

#endif
