#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <binary-sensor.h>
#include <rs-channel.h>

#ifdef CONFIG_BSP_NODE_MCU

static const char *TAG = "BSP";

#define COLOR_SETTINGS_GR "GR"

static setting_t out_settings[] = {
    { .id = "COLOR",
      .label = "MANUAL COLOR",
      .type = SETTING_TYPE_COLOR,
      .color = { { 0xFF, 0xFF, 0xFF } } },
    {} //last element
};

static const settings_group_t board_settings[] = {
    { .id = COLOR_SETTINGS_GR, .label = "GROUP", .settings = out_settings },
    {}
};

static bsp_t brd_nodemcu = { .id = "NodeMCU", .ver = "1.0", .settings_pack = board_settings };

bsp_t *const bsp = &brd_nodemcu;

static supla_channel_t *rs_channel;
static supla_dev_t     *supla_dev;

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
    esp_set_cpu_freq(ESP_CPU_FREQ_160M);
    settings_nvs_read(bsp->settings_pack);
    settings_pack_print(bsp->settings_pack);
    button_init(&btn);
    return ESP_OK;
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    struct rs_channel_config rs_channel_conf = {
        .gpio_fwd = GPIO_NUM_2,
        .gpio_bck = GPIO_NUM_15,
        .default_function = SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER,
        .supported_functions = RS_CH_SUPPORTED_FUNC_BITS //
    };

    rs_channel = supla_rs_channel_create(&rs_channel_conf);
    supla_channel_set_default_caption(rs_channel, "RS");

    supla_dev_add_channel(dev, rs_channel);
    //supla_dev_enable_notifications(dev, 0x00);

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
