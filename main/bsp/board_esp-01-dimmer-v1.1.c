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

#include <pca9557.h>
#include <ledc-channel.h>
#include <exp-input.h>

#ifdef CONFIG_BSP_ESP01_DIMMER_v1_1

#define PCA9536_I2C_ADDR 0x41

#define IN1_SETTINGS_GR "IN1"
#define IN2_SETTINGS_GR "IN2"

static const char *TAG="BSP";

static const char *active_level_labels[] = {
        [ACTIVE_LOW] = "LOW",
        [ACTIVE_HIGH] = "HIGH",
        NULL
};

static setting_t input1_settings[] = {
    {
        .label = "ACTIVE_LVL",
        .type = SETTING_TYPE_ONEOF,
        .oneof = { ACTIVE_HIGH, ACTIVE_HIGH, active_level_labels }
    },{
        .label = "OFF_DELAY",
        .type = SETTING_TYPE_NUM,
        .num = { 1, 1, {1,600} }
    },
    {}
};

static setting_t input2_settings[] = {
    {
        .label = "ACTIVE_LVL",
        .type = SETTING_TYPE_ONEOF,
        .oneof = { ACTIVE_HIGH, ACTIVE_HIGH, active_level_labels }
    },{
        .label = "OFF_DELAY",
        .type = SETTING_TYPE_NUM,
        .num = { 1, 1, {1,600} }
    },
    {}
};

static const settings_group_t board_settings_pack[] = {
        {
            .label = IN1_SETTINGS_GR,
            .settings = input1_settings
        },
        {
            .label = IN2_SETTINGS_GR,
            .settings = input2_settings
        },
        {}
};


static bsp_t brd_esp01_dimmer =
{
    .id = "ESP-01 Dimmer",
    .settings_pack = board_settings_pack
};

bsp_t * const bsp = &brd_esp01_dimmer;

static i2c_dev_t pca9536;

static supla_channel_t *input0_channel;
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
    uint8_t brightness;

    supla_ledc_channel_get_brightness(ledc_channel,&brightness);
    switch (event) {
        case EXP_INPUT_EVENT_INIT:
            if(!(bits & DEVICE_CONFIG_EVENT_BIT)){
                rgbw->brightness = brightness < 25 ? 25 :
                        brightness < 50 ? 50 :
                        brightness < 75 ? 75 :
                        brightness < 100 ? 100 : 0;
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
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value *rgbw = (TRGBW_Value*)&new_value.value;
    uint8_t brightness;
    setting_t *off_delay_set = NULL;
    const char *gr = (pin_num == GPIO_NUM_1) ? IN1_SETTINGS_GR:
                     (pin_num == GPIO_NUM_2) ? IN2_SETTINGS_GR:NULL;

    off_delay_set = settings_pack_find(bsp->settings_pack,gr,"OFF_DELAY");
    EventBits_t bits = device_get_event_bits();
    if(bits & DEVICE_CONFIG_EVENT_BIT)
        return;

    switch (event) {
        case EXP_INPUT_EVENT_INIT:
            ESP_LOGI(TAG,"input %d init",pin_num);
            rgbw->brightness = 100;
            supla_ledc_channel_set_brightness(ledc_channel,&new_value);
            break;
        case EXP_INPUT_EVENT_DONE:
            ESP_LOGI(TAG,"input %d done",pin_num);
            supla_ledc_channel_get_brightness(ledc_channel,&brightness);
            if(brightness){
                new_value.DurationMS = off_delay_set ? 1000 * off_delay_set->num.val : 5000;
                rgbw->brightness = brightness;
                supla_ledc_channel_set_brightness(ledc_channel,&new_value);
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

esp_err_t board_init(supla_dev_t *dev)
{
    setting_t *active_lvl_set = NULL;

    settings_nvs_read(bsp->settings_pack);
    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(pca9557_init_desc(&pca9536, PCA9536_I2C_ADDR, I2C_NUM_0, GPIO_NUM_0, GPIO_NUM_2));

    struct ledc_channel_config ledc_channel_conf = {
        .gpio = GPIO_NUM_3,
        .ledc_channel = LEDC_CHANNEL_0,
        .fade_time = 1000
    };
    ledc_channel = supla_ledc_channel_create(&ledc_channel_conf);

    struct exp_input_config cfg_input_conf = {
        .i2c_expander = &pca9536,
        .pin_num = GPIO_NUM_0,
        .active_level = ACTIVE_LOW,
        .callback = config_btn_calback,
        .action_trigger_caps = SUPLA_ACTION_CAP_TURN_ON,
    };

    active_lvl_set = settings_pack_find(bsp->settings_pack,IN1_SETTINGS_GR,"ACTIVE_LVL");
    struct exp_input_config input1_conf = {
        .i2c_expander = &pca9536,
        .pin_num = GPIO_NUM_1,
        .active_level = active_lvl_set ? active_lvl_set->oneof.val : ACTIVE_HIGH,
        .callback = input_calback,
        .action_trigger_caps = SUPLA_ACTION_CAP_TURN_ON,
        .related_channel = &ledc_channel
    };

    active_lvl_set = settings_pack_find(bsp->settings_pack,IN2_SETTINGS_GR,"ACTIVE_LVL");
    struct exp_input_config input2_conf = {
        .i2c_expander = &pca9536,
        .pin_num = GPIO_NUM_2,
        .active_level = active_lvl_set ? active_lvl_set->oneof.val : ACTIVE_HIGH,
        .callback = input_calback,
        .action_trigger_caps = SUPLA_ACTION_CAP_TURN_ON,
        .related_channel = &ledc_channel
    };

    input0_channel = supla_exp_input_create(&cfg_input_conf);
    input1_channel = supla_exp_input_create(&input1_conf);
    input2_channel = supla_exp_input_create(&input2_conf);

    supla_dev_add_channel(dev,ledc_channel);
    supla_dev_add_channel(dev,input0_channel);
    supla_dev_add_channel(dev,input1_channel);
    supla_dev_add_channel(dev,input2_channel);

    xTaskCreate(&pulse_task, "pulse", 2048, NULL, 1, NULL);
    ESP_LOGI(TAG,"board init completed OK");
    return ESP_OK;
}

esp_err_t board_on_config_mode_init(void)
{
    TSD_SuplaChannelNewValue new_value = {};

    supla_ledc_channel_set_brightness(ledc_channel,&new_value);
    return ESP_OK;
}

esp_err_t board_on_config_mode_exit(void)
{
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value *rgbw = (TRGBW_Value*)&new_value.value;
    rgbw->brightness = 100;
    supla_ledc_channel_set_brightness(ledc_channel,&new_value);
    return ESP_OK;
}

#endif
