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
#include <pir-sensor.h>

#ifdef CONFIG_BSP_ESP01_DIMMER

static const char *TAG="BSP";

static bsp_t brd_esp01_dimmer =
{
    .id = "ESP-01 Dimmer"
};

bsp_t * const bsp = &brd_esp01_dimmer;

static EventGroupHandle_t bsp_event_group;

#define CONFIG_MODE_BIT BIT0

static SemaphoreHandle_t xCountingSemaphore;
static supla_channel_t *ledc_channel;
static supla_channel_t *pir1_channel;
static supla_channel_t *pir2_channel;

static esp_err_t board_config_mode_init(void)
{
    xEventGroupSetBits(bsp_event_group, CONFIG_MODE_BIT);
    device_init_config();
    return ESP_OK;
}

static esp_err_t board_config_mode_exit(void)
{
    TSD_SuplaChannelNewValue new_value = {};

    xEventGroupClearBits(bsp_event_group, CONFIG_MODE_BIT);
    supla_ledc_channel_set_brightness(ledc_channel,&new_value);
    device_exit_config();
    return ESP_OK;
}

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
    EventBits_t bits;

    ESP_LOGI(TAG, "detect_cb event = %s", events[event]);
    ESP_LOGI(TAG, "detect_cb smphr = %d", uxSemaphoreGetCount(xCountingSemaphore));

    bits = xEventGroupWaitBits(bsp_event_group,CONFIG_MODE_BIT,0,0,1);
    switch (event) {
        case INPUT_EVENT_INIT:
            xSemaphoreGive(xCountingSemaphore);
            if(!(bits & CONFIG_MODE_BIT)){
                rgbw->brightness = 100;
                supla_ledc_channel_set_brightness(ch,&new_value);
            }
            break;
        case INPUT_EVENT_CLICKx3:
            if(!(bits & CONFIG_MODE_BIT))
                board_config_mode_init();
            else
                board_config_mode_exit();
            break;
        case INPUT_EVENT_DONE:
            xSemaphoreTake(xCountingSemaphore,portMAX_DELAY);
            if(!(bits & CONFIG_MODE_BIT)){
                if( uxSemaphoreGetCount(xCountingSemaphore) == 0){
                    new_value.DurationMS = 5000;
                    rgbw->brightness = 100;
                    supla_ledc_channel_set_brightness(ch,&new_value);
                }
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

    while(1){
        xEventGroupWaitBits(bsp_event_group,CONFIG_MODE_BIT,0,0,portMAX_DELAY);
        rgbw->brightness = rgbw->brightness ? 0 : 100;
        supla_ledc_channel_set_brightness(ledc_channel,&new_value);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

esp_err_t board_init(supla_dev_t *dev)
{
    bsp_event_group = xEventGroupCreate();
    xCountingSemaphore = xSemaphoreCreateCounting(2,0);
    ledc_channel = supla_ledc_channel_create(GPIO_NUM_3,1000);

    struct pir_sensor_config pir1_conf = {
        .gpio = GPIO_NUM_0,
        .action_trigger_caps =
            SUPLA_ACTION_CAP_TURN_ON |
            SUPLA_ACTION_CAP_TURN_OFF,
        .on_detect_cb = detect_cb,
        .arg = ledc_channel
    };
    struct pir_sensor_config pir2_conf = {
        .gpio = GPIO_NUM_2,
        .action_trigger_caps =
            SUPLA_ACTION_CAP_TURN_ON |
            SUPLA_ACTION_CAP_TURN_OFF,
        .on_detect_cb = detect_cb,
        .arg = ledc_channel
    };
    pir1_channel = supla_pir_sensor_create(&pir1_conf);
    pir2_channel = supla_pir_sensor_create(&pir2_conf);

    supla_dev_set_name(dev,bsp->id);
    supla_dev_add_channel(dev,ledc_channel);
    supla_dev_add_channel(dev,pir1_channel);
    supla_dev_add_channel(dev,pir2_channel);

    xTaskCreate(&pulse_task, "pulse", 2048, NULL, 1, NULL);
    ESP_LOGI(TAG,"board init completed OK");
    return ESP_OK;
}
#endif
