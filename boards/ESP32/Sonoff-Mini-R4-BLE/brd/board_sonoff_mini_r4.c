#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <status-led.h>
#include <generic-input.h>
#include <lampsmart-ble-channel.h>
#ifdef CONFIG_BSP_SONOFF_MINI_R4

#define LED_SETTINGS_GR "LED"
#define BLE_SETTINGS_GR "BLE"

static const char *TAG = "BSP";
static const char *led_state_labels[] = { [0] = "ON", [1] = "OFF", NULL };

static const char *protocol_labels[] = { [LAMPSMART_VARIANT_3] = "V3",
                                         [LAMPSMART_VARIANT_2] = "V2",
                                         [LAMPSMART_VARIANT_1A] = "V1A",
                                         [LAMPSMART_VARIANT_1B] = "V1B",
                                         NULL };

static setting_t led_settings[] = {
    { .id = "ONLINE",
      .label = "ONLINE STATE",
      .type = SETTING_TYPE_ONEOF,
      .oneof = { 0, 0, led_state_labels } },
    {} //last element
};

static setting_t ble_settings[] = {
    { .id = "PROTO",
      .label = "PROTOCOL",
      .type = SETTING_TYPE_ONEOF,
      .oneof = { 0, 0, protocol_labels } },
    { .id = "GROUP_ID",
      .label = "GROUP ID",
      .type = SETTING_TYPE_NUM,
      .num = { 0, 0, { 1, 1024 } } },
    {} //last element
};

static const settings_group_t board_settings_pack[] = {
    { .id = LED_SETTINGS_GR, .label = "Led settings", .settings = led_settings },
    { .id = BLE_SETTINGS_GR, .label = "LampSmart BLE settings", .settings = ble_settings },
    {} //last element
};

static bsp_t brd_sonoff_mini_r4 = { .id = "SONOFF-MINI-R4",
                                    .ver = "1.0",
                                    .settings_pack = board_settings_pack };

bsp_t *const bsp = &brd_sonoff_mini_r4;

static supla_status_led_t *status_led;
static supla_channel_t    *sw_channel;
static supla_channel_t    *ble_channel;

static esp_err_t settings_handler(const settings_group_t *settings, void *arg)
{
    lampsmart_variant_t variant;
    uint32_t            group_id;

    supla_channel_t *ch = arg;

    setting_t *proto_set = settings_pack_find(bsp->settings_pack, BLE_SETTINGS_GR, "PROTO");
    setting_t *group_set = settings_pack_find(bsp->settings_pack, BLE_SETTINGS_GR, "GROUP_ID");

    variant = proto_set ? proto_set->oneof.val : LAMPSMART_VARIANT_3;
    group_id = group_set ? group_set->num.val : 0x1234;

    lamp_ble_channel_set_protocol(ch, variant);
    lamp_ble_channel_set_group_id(ch, group_id);

    ESP_LOGI(TAG, "Settings updated: pairing protocol=%s group_id=0x%08" PRIX32,
             protocol_labels[variant], group_id);

    lamp_ble_channel_pair(ble_channel);
    return ESP_OK;
}

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

static void switch_cb(gpio_num_t pin_num, input_event_t event, void *arg)
{
    EventBits_t bits = device_get_event_bits();

    const char *events[] = {
        [INPUT_EVENT_INIT] = "INPUT_EVENT_INIT",      [INPUT_EVENT_CLICK1] = "INPUT_EVENT_CLICKx1",
        [INPUT_EVENT_CLICK2] = "INPUT_EVENT_CLICKx2", [INPUT_EVENT_CLICK3] = "INPUT_EVENT_CLICKx3",
        [INPUT_EVENT_CLICK4] = "INPUT_EVENT_CLICKx4", [INPUT_EVENT_CLICK5] = "INPUT_EVENT_CLICKx5",
        [INPUT_EVENT_DONE] = "INPUT_EVENT_DONE"
    };

    ESP_LOGI(TAG, "input event GPIO %d: %s", pin_num, events[event]);
    switch (event) {
    case INPUT_EVENT_INIT:
        break;
    case INPUT_EVENT_CLICK3:
        if (!(bits & DEVICE_CONFIG_EVENT_BIT))
            device_init_config();
        else
            device_exit_config();
        break;
    case INPUT_EVENT_DONE:
        TSD_SuplaChannelNewValue new_value = {};
        TRGBW_Value             *rgbw = (TRGBW_Value *)&new_value.value;

        TRGBW_Value value_now;
        lamp_ble_channel_get_value(ble_channel, &value_now);
        if (value_now.brightness > 0) {
            rgbw->brightness = 0;
        } else {
            rgbw->brightness = 100;
            rgbw->whiteTemperature = 0;
        }
        lamp_ble_channel_set_value(ble_channel, &new_value);
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

    struct lamp_ble_channel_config ble_channel_conf = {
        .lamp_config = LAMPSMART_BLE_CONFIG_DEFAULT(), //
        .relay_gpio = GPIO_NUM_26                      //
    };
    ble_channel = lamp_ble_channel_create(&ble_channel_conf);

    struct generic_input_config sw_conf = {
        .gpio = GPIO_NUM_27,
        .action_trigger_caps = SUPLA_ACTION_CAP_SHORT_PRESS_x1 | SUPLA_ACTION_CAP_SHORT_PRESS_x2 |
                               SUPLA_ACTION_CAP_SHORT_PRESS_x3 | SUPLA_ACTION_CAP_SHORT_PRESS_x4 |
                               SUPLA_ACTION_CAP_SHORT_PRESS_x5 | SUPLA_ACTION_CAP_HOLD,
        .related_channel = &ble_channel,
        .on_event_cb = switch_cb,
        .arg = ble_channel //
    };
    sw_channel = supla_generic_input_create(&sw_conf);

    supla_dev_add_channel(dev, ble_channel);
    supla_dev_add_channel(dev, sw_channel);
    settings_handler_register(settings_handler, ble_channel);
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
