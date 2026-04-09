#include <inttypes.h>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "lampsmart_ble.h"

static const char *TAG = "example_lampsmart";

static const char *variant_name(lampsmart_variant_t variant)
{
    switch (variant) {
    case LAMPSMART_VARIANT_3:
        return "V3";
    case LAMPSMART_VARIANT_2:
        return "V2";
    case LAMPSMART_VARIANT_1A:
        return "V1A";
    case LAMPSMART_VARIANT_1B:
        return "V1B";
    default:
        return "UNKNOWN";
    }
}

static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS init returned %s, erasing and retrying", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    nvs_init();

    lampsmart_ble_t        light = NULL;
    lampsmart_ble_config_t cfg = LAMPSMART_BLE_CONFIG_DEFAULT();
    cfg.group_id = 0x12345678;
    cfg.variant = LAMPSMART_VARIANT_3;
    cfg.tx_duration_ms = 1000;
    cfg.reversed_channels = false;

    ESP_LOGI(TAG, "Config: group_id=0x%08" PRIX32 " variant=%s duration_ms=%" PRIu32 " reversed=%d",
             cfg.group_id, variant_name(cfg.variant), cfg.tx_duration_ms, cfg.reversed_channels);

    ESP_ERROR_CHECK(lampsmart_ble_init(&light, &cfg));
    ESP_LOGI(TAG, "LampSmart controller initialized for group: 0x%08" PRIX32, cfg.group_id);

    ESP_LOGI(TAG, "Put lamp in pairing mode now. Pairing using variant=%s",
             variant_name(cfg.variant));
    ESP_ERROR_CHECK(lampsmart_ble_pair(light));

    while (true) {
        ESP_LOGI(TAG, "Set levels request: cold=200 warm=40 variant=%s", variant_name(cfg.variant));
        ESP_ERROR_CHECK(lampsmart_ble_set_levels(light, 200, 40));
        vTaskDelay(pdMS_TO_TICKS(2500));

        ESP_LOGI(TAG, "Set levels request: cold=30 warm=190 variant=%s", variant_name(cfg.variant));
        ESP_ERROR_CHECK(lampsmart_ble_set_levels(light, 30, 190));
        vTaskDelay(pdMS_TO_TICKS(2500));

        for (int i = 0; i < 25; i++) {
            uint8_t val = 10 * i;
            ESP_LOGI(TAG, "Set levels request: cold=%d warm=%d  variant=%s", val, val,
                     variant_name(cfg.variant));

            ESP_ERROR_CHECK(lampsmart_ble_set_levels(light, val, val));
            vTaskDelay(pdMS_TO_TICKS(2500));
        }

        ESP_LOGI(TAG, "Turn OFF request variant=%s", variant_name(cfg.variant));
        ESP_ERROR_CHECK(lampsmart_ble_turn_off(light));
        vTaskDelay(pdMS_TO_TICKS(2000));

        ESP_LOGI(TAG, "Turn ON request variant=%s", variant_name(cfg.variant));
        ESP_ERROR_CHECK(lampsmart_ble_turn_on(light));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
