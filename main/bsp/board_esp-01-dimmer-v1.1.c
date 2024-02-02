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

#include <pca9557.h>
#include <ledc-channel.h>
#include <exp-input.h>

#ifdef CONFIG_BSP_ESP01_DIMMER_v1_1

#define PCA9536_I2C_ADDR 0x41

static const char *TAG="BSP";

static bsp_t brd_esp01_dimmer =
{
    .id = "ESP-01 Dimmer"
};

bsp_t * const bsp = &brd_esp01_dimmer;

static i2c_dev_t pca9536;
static exp_input_t exp_input_config;
static exp_input_t exp_input1;
static exp_input_t exp_input2;

static supla_channel_t *input1_channel;
static supla_channel_t *input2_channel;
static supla_channel_t *ledc_channel;

esp_err_t board_early_init(void)
{
    return ESP_OK;
}

static void config_btn_calback(gpio_num_t pin_num,exp_input_event_t event,void *arg)
{
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value *rgbw = (TRGBW_Value*)&new_value.value;
    EventBits_t bits = device_get_event_bits();

    switch (event) {
        case EXP_INPUT_EVENT_INIT:
            if(!(bits & DEVICE_CONFIG_EVENT_BIT)){
                new_value.DurationMS = 5000;
                rgbw->brightness = 100;
                supla_ledc_channel_set_brightness(ledc_channel,&new_value);
            }break;
        case EXP_INPUT_EVENT_DONE:
            break;
        case EXP_INPUT_EVENT_HOLD:
            if(!(bits & DEVICE_CONFIG_EVENT_BIT))
                 device_init_config();
             else
                 device_exit_config();
            break;
        default:
            break;
    }
}

static void input_calback(gpio_num_t pin_num,exp_input_event_t event,void *arg)
{
    supla_channel_t *at_channel = arg;
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value *rgbw = (TRGBW_Value*)&new_value.value;

    EventBits_t bits = device_get_event_bits();
    if(bits & DEVICE_CONFIG_EVENT_BIT)
        return;

    switch (event) {
        case EXP_INPUT_EVENT_INIT:
            ESP_LOGI(TAG,"input %d init",pin_num);
            rgbw->brightness = 100;
            supla_ledc_channel_set_brightness(ledc_channel,&new_value);
            supla_channel_emit_action(at_channel,SUPLA_ACTION_CAP_TURN_ON);
            break;
        case EXP_INPUT_EVENT_DONE:
            ESP_LOGI(TAG,"input %d done",pin_num);
            new_value.DurationMS = 5000;
            rgbw->brightness = 100;
            supla_ledc_channel_set_brightness(ledc_channel,&new_value);
            supla_channel_emit_action(at_channel,SUPLA_ACTION_CAP_TURN_OFF);
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

esp_err_t board_init(supla_dev_t *dev)
{
    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(pca9557_init_desc(&pca9536, PCA9536_I2C_ADDR, I2C_NUM_0, GPIO_NUM_0, GPIO_NUM_2));

    struct ledc_channel_config ledc_channel_conf = {
        .gpio = GPIO_NUM_3,
        .ledc_channel = LEDC_CHANNEL_0,
        .fade_time = 1000
    };
    ledc_channel = supla_ledc_channel_create(&ledc_channel_conf);

    supla_channel_config_t at_channel_config = {
        .type = SUPLA_CHANNELTYPE_ACTIONTRIGGER,
        .supported_functions = SUPLA_CHANNELFNC_ACTIONTRIGGER,
        .default_function = SUPLA_CHANNELFNC_ACTIONTRIGGER,
        .action_trigger_caps = SUPLA_ACTION_CAP_TURN_ON|SUPLA_ACTION_CAP_TURN_OFF,
        .action_trigger_related_channel = &ledc_channel
    };

    input1_channel = supla_channel_create(&at_channel_config);
    input2_channel = supla_channel_create(&at_channel_config);

    struct exp_input_config cfg_input_conf = {
        .i2c_expander = &pca9536,
        .pin_num = GPIO_NUM_0,
        .active_level = ACTIVE_LOW,
        .callback = config_btn_calback
    };

    struct exp_input_config input1_conf = {
        .i2c_expander = &pca9536,
        .pin_num = GPIO_NUM_1,
        .active_level = ACTIVE_HIGH,
        .callback = input_calback,
        .arg = input1_channel
    };

    struct exp_input_config input2_conf = {
        .i2c_expander = &pca9536,
        .pin_num = GPIO_NUM_2,
        .active_level = ACTIVE_HIGH,
        .callback = input_calback,
        .arg = input2_channel
    };

    exp_input_config = exp_input_create(&cfg_input_conf);
    exp_input1 = exp_input_create(&input1_conf);
    exp_input2 = exp_input_create(&input2_conf);

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
