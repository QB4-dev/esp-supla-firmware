#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <lampsmart-ble-channel.h>
#include <relay-channel.h>

#ifdef CONFIG_BSP_ESP32

static const char *TAG = "BSP";

#define BLE_SETTINGS_GR "BLE"

static const char *protocol_labels[] = { [LAMPSMART_VARIANT_3] = "V3",
                                         [LAMPSMART_VARIANT_2] = "V2",
                                         [LAMPSMART_VARIANT_1A] = "V1A",
                                         [LAMPSMART_VARIANT_1B] = "V1B",
                                         NULL };

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
    { .id = BLE_SETTINGS_GR, .label = "BLE settings", .settings = ble_settings },
    {} //last element
};

static bsp_t brd_nodemcu = { .id = "ESP32-BLE",
                             .ver = "1.0",
                             .settings_pack = board_settings_pack };

bsp_t *const bsp = &brd_nodemcu;

static supla_channel_t *ble_channel;

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
        ESP_LOGI(TAG, "btn clicked - pairing BLE channel");
        lamp_ble_channel_pair(ble_channel);
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
    settings_nvs_read(bsp->settings_pack);
    settings_pack_print(bsp->settings_pack);
    button_init(&btn);
    return ESP_OK;
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    struct lamp_ble_channel_config ble_channel_conf = {
        .lamp_config = LAMPSMART_BLE_CONFIG_DEFAULT(),
    };

    ble_channel = lamp_ble_channel_create(&ble_channel_conf);
    supla_dev_add_channel(dev, ble_channel);

    settings_handler_register(settings_handler, ble_channel);
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
