#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <status-led.h>
#include <generic-input.h>
#include <relay-channel.h>
#ifdef CONFIG_BSP_SONOFF_MINI_R4

#define LED_SETTINGS_GR "LED"

static const char *TAG = "BSP";
static const char *led_state_labels[] = { [0] = "ON", [1] = "OFF", NULL };

static setting_t led_settings[] = {
    { .id = "ONLINE",
      .label = "ONLINE STATE",
      .type = SETTING_TYPE_ONEOF,
      .oneof = { 0, 0, led_state_labels } },
    {} //last element
};

static const settings_group_t board_settings_pack[] = {
    { .id = LED_SETTINGS_GR, .label = "Led settings", .settings = led_settings },
    {} //last element
};

static bsp_t brd_sonoff_mini_r4 = { .id = "SONOFF-MINI-R4",
                                    .ver = "1.0",
                                    .settings_pack = board_settings_pack };

bsp_t *const bsp = &brd_sonoff_mini_r4;

static supla_status_led_t *status_led;
static supla_channel_t    *sw_channel;
static supla_channel_t    *relay_channel;

static void button_cb(button_t *btn, button_state_t state)
{
    TRelayChannel_Value relay_value = {};
    EventBits_t         bits = device_get_event_bits();
    switch (state) {
    case BUTTON_CLICKED:
        ESP_LOGI(TAG, "btn clicked");
        relay_value.hi = !supla_relay_channel_get_state(relay_channel);
        supla_relay_channel_set_local(relay_channel, &relay_value);
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

static void switch_cb(gpio_num_t pin_num, input_event_t event, void *arg)
{
    TRelayChannel_Value relay_value = {};

    const char *events[] = {
        [INPUT_EVENT_INIT] = "INPUT_EVENT_INIT",      [INPUT_EVENT_CLICK1] = "INPUT_EVENT_CLICKx1",
        [INPUT_EVENT_CLICK2] = "INPUT_EVENT_CLICKx2", [INPUT_EVENT_CLICK3] = "INPUT_EVENT_CLICKx3",
        [INPUT_EVENT_CLICK4] = "INPUT_EVENT_CLICKx4", [INPUT_EVENT_CLICK5] = "INPUT_EVENT_CLICKx5",
        [INPUT_EVENT_HOLD] = "INPUT_EVENT_HOLD",      [INPUT_EVENT_DONE] = "INPUT_EVENT_DONE"
    };

    ESP_LOGI(TAG, "input event GPIO %d: %s", pin_num, events[event]);
    switch (event) {
    case INPUT_EVENT_CLICK1:
        relay_value.hi = !supla_relay_channel_get_state(relay_channel);
        supla_relay_channel_set_local(relay_channel, &relay_value);
        break;
    case INPUT_EVENT_HOLD:
        break;
    default:
        break;
    }
}

esp_err_t board_early_init(void)
{
    settings_nvs_read(bsp->settings_pack);
    return ESP_OK;
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    setting_t *led_setting = settings_pack_find(bsp->settings_pack, LED_SETTINGS_GR, "ONLINE");

    static button_t config_btn_conf = {
        .gpio = GPIO_NUM_0, .callback = button_cb //
    };
    button_init(&config_btn_conf);

    struct status_led_config led_conf = {
        .gpio = GPIO_NUM_19,
        .online_set = led_setting ? led_setting->boolean.val : false //
    };
    status_led = supla_status_led_init(dev, &led_conf);

    struct relay_channel_config relay_channel_conf = {
        .gpio = GPIO_NUM_26,
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .supported_functions = RELAY_CH_SUPPORTED_FUNC_BITS
    };
    relay_channel = supla_relay_channel_create(&relay_channel_conf);

    struct generic_input_config sw_conf = {
        .gpio = GPIO_NUM_27,
        .active_level = ACTIVE_LOW,
        .pull_mode = PULL_UP,
        .action_trigger_caps = SUPLA_ACTION_CAP_SHORT_PRESS_x1 | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
                               SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
                               SUPLA_ACTION_CAP_SHORT_PRESS_x5 | SUPLA_ACTION_CAP_HOLD,
        .related_channel = &relay_channel,
        .on_event_cb = switch_cb,
        .arg = relay_channel //
    };
    sw_channel = supla_generic_input_create(&sw_conf);

    supla_dev_add_channel(dev, relay_channel);
    supla_dev_add_channel(dev, sw_channel);
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

#endif /* CONFIG_BSP_SONOFF_MINI_R4 */
