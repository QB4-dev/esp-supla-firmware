#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <pca9557.h>
#include <generic-input.h>
#include <exp-input.h>
#include <ledc-channel.h>

#define IN1_SETTINGS_GR "IN1"
#define IN2_SETTINGS_GR "IN2"
#define REDUCTION_GR "BRCTL"
#define PAUSE_GR "PAUSE"

static const char *TAG = "BSP";
static const char *active_level_labels[] = { [ACTIVE_LOW] = "LOW", [ACTIVE_HIGH] = "HIGH", NULL };

static setting_t input1_settings[] = {
    { .id = "ACTIVE_LVL",
      .label = "ACTIVE LEVEL",
      .type = SETTING_TYPE_ONEOF,
      .oneof = { ACTIVE_HIGH, ACTIVE_HIGH, active_level_labels } },
    { .id = "OFF_DELAY",
      .label = "OFF DELAY",
      .type = SETTING_TYPE_NUM,
      .num = { 10, 10, { 1, 600 } } },
    {} //last element
};

static setting_t input2_settings[] = {
    { .id = "ACTIVE_LVL",
      .label = "ACTIVE LEVEL",
      .type = SETTING_TYPE_ONEOF,
      .oneof = { ACTIVE_HIGH, ACTIVE_HIGH, active_level_labels } },
    { .id = "OFF_DELAY",
      .label = "OFF DELAY",
      .type = SETTING_TYPE_NUM,
      .num = { 10, 10, { 1, 600 } } },
    {} //last element
};

static setting_t pause_settings[] = {
    { .id = "EN", .label = "ENABLED", .type = SETTING_TYPE_BOOL, .boolean = { false, false } },
    { .id = "FROM", .label = "FROM", .type = SETTING_TYPE_TIME, .time = { 8, 00 } },
    { .id = "TO", .label = "TO", .type = SETTING_TYPE_TIME, .time = { 17, 00 } },
    {} //last element
};

static setting_t brightness_settings[] = {
    { .id = "EN", .label = "ENABLED", .type = SETTING_TYPE_BOOL, .boolean = { false, false } },
    { .id = "BR", .label = "BRIGHTNESS", .type = SETTING_TYPE_NUM, .num = { 100, 100, { 0, 100 } } },
    { .id = "FROM", .label = "FROM", .type = SETTING_TYPE_TIME, .time = { 21, 00 } },
    { .id = "TO", .label = "TO", .type = SETTING_TYPE_TIME, .time = { 4, 00 } },
    {} //last element
};

static const settings_group_t board_settings[] = {
    { .id = IN1_SETTINGS_GR, .label = IN1_SETTINGS_GR, .settings = input1_settings },
    { .id = IN2_SETTINGS_GR, .label = IN2_SETTINGS_GR, .settings = input2_settings },
    { .id = PAUSE_GR, .label = "PAUSE", .settings = pause_settings },
    { .id = REDUCTION_GR, .label = "REDUCE BRIGHTNESS", .settings = brightness_settings },
    {}
};

#ifdef CONFIG_BSP_ESP01_DIMMER_v1_0
// Global variables for board v1.0
static bsp_t brd_esp01_dimmer = { .id = "ESP01_DIMMER",
                                  .ver = "1.0",
                                  .settings_pack = board_settings };

static supla_channel_t *ledc_channel;
static supla_channel_t *input1_channel;
static supla_channel_t *input2_channel;
#endif //CONFIG_BSP_ESP01_DIMMER_v1_0

#ifdef CONFIG_BSP_ESP01_DIMMER_v1_1
// Global variables for board v1.1
static bsp_t brd_esp01_dimmer = { .id = "ESP01_DIMMER",
                                  .ver = "1.1",
                                  .settings_pack = board_settings };

static i2c_dev_t        pca9536;
static supla_channel_t *input0_channel;
static supla_channel_t *input1_channel;
static supla_channel_t *input2_channel;
static supla_channel_t *ledc_channel;
#endif //CONFIG_BSP_ESP01_DIMMER_v1_1

#ifdef CONFIG_BSP_ESP01_DIMMER_v1_2
// Global variables for board v1.2
static bsp_t            brd_esp01_dimmer = { .id = "ESP01_DIMMER",
                                             .ver = "1.2",
                                             .settings_pack = board_settings };
static i2c_dev_t        pca9536;
static supla_channel_t *input0_channel;
static supla_channel_t *input1_channel;
static supla_channel_t *input2_channel;
static supla_channel_t *ledc_channel;
#endif //CONFIG_BSP_ESP01_DIMMER_v1_2

bsp_t *const bsp = &brd_esp01_dimmer;

esp_err_t board_early_init(void)
{
    esp_set_cpu_freq(ESP_CPU_FREQ_80M);
    settings_nvs_read(bsp->settings_pack);
    //settings_pack_print(bsp->settings_pack);
    return ESP_OK;
}

static int time_is_between(int start_hh, int start_mm, int end_hh, int end_mm)
{
    time_t     now = time(NULL);
    struct tm *current_time = localtime(&now);

    // Current time in minutes since midnight
    int current_minutes = current_time->tm_hour * 60 + current_time->tm_min;

    // Start time in minutes since midnight
    int start_minutes = start_hh * 60 + start_mm;

    // End time in minutes since midnight
    int end_minutes = end_hh * 60 + end_mm;

    if (start_minutes <= end_minutes)
        return current_minutes >= start_minutes && current_minutes <= end_minutes;
    else
        return current_minutes >= start_minutes || current_minutes <= end_minutes;
}

static bool brightness_reduction_is_active(void)
{
    setting_t *enabled = settings_pack_find(bsp->settings_pack, REDUCTION_GR, "EN");
    setting_t *from = settings_pack_find(bsp->settings_pack, REDUCTION_GR, "FROM");
    setting_t *to = settings_pack_find(bsp->settings_pack, REDUCTION_GR, "TO");

    if (!enabled || !enabled->boolean.val || !from || !to)
        return false;

    return time_is_between(from->time.hh, from->time.mm, to->time.hh, to->time.mm);
}

static bool pause_is_active(void)
{
    setting_t *enabled = settings_pack_find(bsp->settings_pack, PAUSE_GR, "EN");
    setting_t *from = settings_pack_find(bsp->settings_pack, PAUSE_GR, "FROM");
    setting_t *to = settings_pack_find(bsp->settings_pack, PAUSE_GR, "TO");

    if (!enabled || !enabled->boolean.val || !from || !to)
        return false;

    return time_is_between(from->time.hh, from->time.mm, to->time.hh, to->time.mm);
}

static void pulse_task(void *arg)
{
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value             *rgbw = (TRGBW_Value *)&new_value.value;
    EventBits_t              bits;

    while (1) {
        bits = device_get_event_bits();
        if (bits & DEVICE_CONFIG_EVENT_BIT) {
            rgbw->brightness = rgbw->brightness ? 0 : 100;
            supla_ledc_channel_set_brightness(ledc_channel, &new_value);
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

#if defined CONFIG_BSP_ESP01_DIMMER_v1_0
static void detect_cb(gpio_num_t pin_num, input_event_t event, void *arg)
{
    const char *events[] = { [INPUT_EVENT_INIT] = "INPUT_EVENT_INIT",
                             [INPUT_EVENT_CLICKx1] = "INPUT_EVENT_CLICKx1",
                             [INPUT_EVENT_CLICKx2] = "INPUT_EVENT_CLICKx2",
                             [INPUT_EVENT_CLICKx3] = "INPUT_EVENT_CLICKx3",
                             [INPUT_EVENT_CLICKx4] = "INPUT_EVENT_CLICKx4",
                             [INPUT_EVENT_CLICKx5] = "INPUT_EVENT_CLICKx5",
                             [INPUT_EVENT_DONE] = "INPUT_EVENT_DONE" };

    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value             *rgbw = (TRGBW_Value *)&new_value.value;
    uint8_t                  brightness;
    EventBits_t              bits = device_get_event_bits();

    const char *gr = (pin_num == GPIO_NUM_0) ? IN1_SETTINGS_GR :
                     (pin_num == GPIO_NUM_2) ? IN2_SETTINGS_GR :
                                               NULL;

    setting_t *off_delay_set = settings_pack_find(bsp->settings_pack, gr, "OFF_DELAY");
    setting_t *reduced_br_set = settings_pack_find(bsp->settings_pack, REDUCTION_GR, "BR");

    ESP_LOGI(TAG, "input event GPIO %d: %s", pin_num, events[event]);
    switch (event) {
    case INPUT_EVENT_INIT:
        if (!(bits & DEVICE_CONFIG_EVENT_BIT)) {
            if (pause_is_active()) {
                ESP_LOGW(TAG, "pause active");
                break;
            }

            if (brightness_reduction_is_active()) {
                rgbw->brightness = reduced_br_set ? reduced_br_set->num.val : 100;
                ESP_LOGW(TAG, "reduced brightness to %d", rgbw->brightness);
            } else {
                rgbw->brightness = 100;
            }
            supla_ledc_channel_set_brightness(ledc_channel, &new_value);
        }
        break;
    case INPUT_EVENT_CLICKx3:
        if (!(bits & DEVICE_CONFIG_EVENT_BIT))
            device_init_config();
        else
            device_exit_config();
        break;
    case INPUT_EVENT_DONE:
        supla_ledc_channel_get_brightness(ledc_channel, &brightness);
        if (brightness) {
            new_value.DurationMS = off_delay_set ? 1000 * off_delay_set->num.val : 5000;
            rgbw->brightness = brightness;
            supla_ledc_channel_set_brightness(ledc_channel, &new_value);
        }
        break;
    default:
        break;
    }
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    struct ledc_channel_config ledc_channel_conf = { .gpio = GPIO_NUM_3,
                                                     .ledc_channel = LEDC_CHANNEL_0,
                                                     .fade_time = 1000 };
    ledc_channel = supla_ledc_channel_create(&ledc_channel_conf);

    struct generic_input_config in1_conf = { .gpio = GPIO_NUM_0,
                                             .action_trigger_caps = SUPLA_ACTION_CAP_TURN_ON |
                                                                    SUPLA_ACTION_CAP_TURN_OFF,
                                             .related_channel = &ledc_channel,
                                             .on_detect_cb = detect_cb,
                                             .arg = ledc_channel };
    struct generic_input_config in2_conf = { .gpio = GPIO_NUM_2,
                                             .action_trigger_caps = SUPLA_ACTION_CAP_TURN_ON |
                                                                    SUPLA_ACTION_CAP_TURN_OFF,
                                             .related_channel = &ledc_channel,
                                             .on_detect_cb = detect_cb,
                                             .arg = ledc_channel };

    input1_channel = supla_generic_input_create(&in1_conf);
    input2_channel = supla_generic_input_create(&in2_conf);

    supla_dev_set_name(dev, bsp->id);
    supla_dev_add_channel(dev, ledc_channel);
    supla_dev_add_channel(dev, input1_channel);
    supla_dev_add_channel(dev, input2_channel);

    xTaskCreate(&pulse_task, "pulse", 2048, NULL, 1, NULL);
    ESP_LOGI(TAG, "board init completed OK");
    return ESP_OK;
}

#endif //CONFIG_BSP_ESP01_DIMMER_v1_0

#if defined CONFIG_BSP_ESP01_DIMMER_v1_1 || defined CONFIG_BSP_ESP01_DIMMER_v1_2
static void config_btn_calback(gpio_num_t pin_num, exp_input_event_t event, void *arg)
{
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value             *rgbw = (TRGBW_Value *)&new_value.value;
    EventBits_t              bits = device_get_event_bits();
    uint8_t                  brightness;

    supla_ledc_channel_get_brightness(ledc_channel, &brightness);
    switch (event) {
    case EXP_INPUT_EVENT_INIT:
        if (!(bits & DEVICE_CONFIG_EVENT_BIT)) {
            rgbw->brightness = brightness < 25  ? 25 :
                               brightness < 50  ? 50 :
                               brightness < 75  ? 75 :
                               brightness < 100 ? 100 :
                                                  0;
            supla_ledc_channel_set_brightness(ledc_channel, &new_value);
        }
        break;
    case EXP_INPUT_EVENT_DONE:
        break;
    case EXP_INPUT_EVENT_HOLD:
        if (!(bits & DEVICE_CONFIG_EVENT_BIT))
            device_init_config();
        else
            device_exit_config();
        break;
    default:
        break;
    }
}

static void input_calback(gpio_num_t pin_num, exp_input_event_t event, void *arg)
{
    TSD_SuplaChannelNewValue new_value = {};
    TRGBW_Value             *rgbw = (TRGBW_Value *)&new_value.value;
    uint8_t                  brightness;
    const char              *gr = (pin_num == GPIO_NUM_1) ? IN1_SETTINGS_GR :
                                  (pin_num == GPIO_NUM_2) ? IN2_SETTINGS_GR :
                                                            NULL;

    setting_t *off_delay_set = settings_pack_find(bsp->settings_pack, gr, "OFF_DELAY");
    setting_t *reduced_br_set = settings_pack_find(bsp->settings_pack, REDUCTION_GR, "BR");

    EventBits_t bits = device_get_event_bits();
    if (bits & DEVICE_CONFIG_EVENT_BIT)
        return;

    switch (event) {
    case EXP_INPUT_EVENT_INIT:
        ESP_LOGI(TAG, "input %d init", pin_num);
        if (pause_is_active()) {
            ESP_LOGW(TAG, "pause active");
            break;
        }

        if (brightness_reduction_is_active()) {
            rgbw->brightness = reduced_br_set ? reduced_br_set->num.val : 100;
            ESP_LOGW(TAG, "reduced brightness to %d", rgbw->brightness);
        } else {
            rgbw->brightness = 100;
        }
        supla_ledc_channel_set_brightness(ledc_channel, &new_value);
        break;
    case EXP_INPUT_EVENT_DONE:
        ESP_LOGI(TAG, "input %d done", pin_num);
        supla_ledc_channel_get_brightness(ledc_channel, &brightness);
        if (brightness) {
            new_value.DurationMS = off_delay_set ? 1000 * off_delay_set->num.val : 5000;
            rgbw->brightness = brightness;
            supla_ledc_channel_set_brightness(ledc_channel, &new_value);
        }
        break;
    default:
        break;
    }
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    setting_t *active_lvl_set = NULL;

    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(
        pca9557_init_desc(&pca9536, PCA9536_I2C_ADDR, I2C_NUM_0, GPIO_NUM_0, GPIO_NUM_2));

    struct ledc_channel_config ledc_channel_conf = {
        .gpio = GPIO_NUM_3,
        .ledc_channel = LEDC_CHANNEL_0,
        .fade_time = 1000 //
    };
    ledc_channel = supla_ledc_channel_create(&ledc_channel_conf);

    struct exp_input_config cfg_input_conf = {
        .i2c_expander = &pca9536,
        .pin_num = GPIO_NUM_0,
        .active_level = ACTIVE_LOW,
        .callback = config_btn_calback,
        .action_trigger_caps = SUPLA_ACTION_CAP_TURN_ON,
    };

    active_lvl_set = settings_pack_find(bsp->settings_pack, IN1_SETTINGS_GR, "ACTIVE_LVL");
    struct exp_input_config input1_conf = {
        .i2c_expander = &pca9536,
        .pin_num = GPIO_NUM_1,
        .active_level = active_lvl_set ? active_lvl_set->oneof.val : ACTIVE_HIGH,
        .callback = input_calback,
        .action_trigger_caps = SUPLA_ACTION_CAP_TURN_ON,
        .related_channel = &ledc_channel //
    };

    active_lvl_set = settings_pack_find(bsp->settings_pack, IN2_SETTINGS_GR, "ACTIVE_LVL");
    struct exp_input_config input2_conf = {
        .i2c_expander = &pca9536,
        .pin_num = GPIO_NUM_2,
        .active_level = active_lvl_set ? active_lvl_set->oneof.val : ACTIVE_HIGH,
        .callback = input_calback,
        .action_trigger_caps = SUPLA_ACTION_CAP_TURN_ON,
        .related_channel = &ledc_channel //
    };

    input0_channel = supla_exp_input_create(&cfg_input_conf);
    input1_channel = supla_exp_input_create(&input1_conf);
    input2_channel = supla_exp_input_create(&input2_conf);

    supla_dev_add_channel(dev, ledc_channel);
    supla_dev_add_channel(dev, input0_channel);
    supla_dev_add_channel(dev, input1_channel);
    supla_dev_add_channel(dev, input2_channel);

    xTaskCreate(&pulse_task, "pulse", 2048, NULL, tskIDLE_PRIORITY, NULL);
    ESP_LOGI(TAG, "%s v%s  init completed OK", bsp->id, bsp->ver);
    return ESP_OK;
}
#endif // CONFIG_BSP_ESP01_DIMMER_v1_1 || CONFIG_BSP_ESP01_DIMMER_v1_2

esp_err_t board_on_config_mode_init(void)
{
    TSD_SuplaChannelNewValue new_value = {};

    supla_ledc_channel_set_brightness(ledc_channel, &new_value);
    return ESP_OK;
}

esp_err_t board_on_config_mode_exit(void)
{
    TSD_SuplaChannelNewValue new_value = {};

    supla_ledc_channel_set_brightness(ledc_channel, &new_value);
    return ESP_OK;
}
