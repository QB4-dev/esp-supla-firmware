#include <sdkconfig.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include "freertos/event_groups.h"

#include <board.h>
#include <button.h>
#include <status-led-rgb.h>
#include <relay-channel.h>

#include <dht-sensor.h>
#include <link-thermometer.h>
#include <thermostat-channel.h>
#include <relay.h>

static const char *TAG = "BSP";

#define CHANNELS_COUNT 6

#ifdef CONFIG_BSP_RELAY_6CH_ENABLE_HVAC

#define THERMOMETER_SETTINGS_ID "THERM"
#define MODE_SETTING_ID "MODE"
#define CAPTION_SETTING_ID "TH-CAP"
#define LINK_SETTING_ID "TH-LINK"
#define CHANNEL_MODE_RELAY 0
#define CHANNEL_MODE_THERMOSTAT 1

static char thermometer_captions[CHANNELS_COUNT][64];
static char thermometer_links[CHANNELS_COUNT][256];
static const char *channel_mode_options[] = { "RELAY", "THERMOSTAT", NULL };

#define DEFINE_CHANNEL_SETTINGS(index, number)                                               \
    static setting_t channel_##number##_settings[] = {                                      \
        { .id = MODE_SETTING_ID,                                                            \
          .label = "Mode",                                                                  \
          .type = SETTING_TYPE_ONEOF,                                                       \
          .oneof = { .val = CHANNEL_MODE_RELAY, .def = CHANNEL_MODE_RELAY,                  \
                     .options = channel_mode_options } },                                  \
        { .id = CAPTION_SETTING_ID,                                                         \
          .label = "Thermometer caption",                                                   \
          .type = SETTING_TYPE_TEXT,                                                        \
          .text = { .val = thermometer_captions[index], .def = "Thermometer " #number,     \
                    .len = sizeof(thermometer_captions[index]) } },                        \
        { .id = LINK_SETTING_ID,                                                            \
          .label = "Thermometer link",                                                      \
          .type = SETTING_TYPE_TEXT,                                                        \
          .text = { .val = thermometer_links[index], .def = "",                            \
                    .len = sizeof(thermometer_links[index]) } },                           \
        {}                                                                                 \
    }

DEFINE_CHANNEL_SETTINGS(0, 1);
DEFINE_CHANNEL_SETTINGS(1, 2);
DEFINE_CHANNEL_SETTINGS(2, 3);
DEFINE_CHANNEL_SETTINGS(3, 4);
DEFINE_CHANNEL_SETTINGS(4, 5);
DEFINE_CHANNEL_SETTINGS(5, 6);

#define CHANNEL_SETTINGS_GROUP(number)                                                       \
    { .id = "CH" #number, .label = "Channel " #number,                                     \
      .settings = channel_##number##_settings }

static const settings_group_t board_settings_pack[] = {
    CHANNEL_SETTINGS_GROUP(1),
    CHANNEL_SETTINGS_GROUP(2),
    CHANNEL_SETTINGS_GROUP(3),
    CHANNEL_SETTINGS_GROUP(4),
    CHANNEL_SETTINGS_GROUP(5),
    CHANNEL_SETTINGS_GROUP(6),
    {},
};

static const char *const channel_group_ids[CHANNELS_COUNT] = {
    "CH1", "CH2", "CH3", "CH4", "CH5", "CH6",
};

struct thermostat_channel_user_ctx {
    supla_channel_t *local_thermometer;
    supla_channel_t *link_thermometer;
    relay_handle_t   relay;
};

static supla_channel_t *local_temp_channel;
static struct thermostat_channel_user_ctx thermostat_channel_user_ctx[CHANNELS_COUNT];

static setting_t *channel_setting(int channel_index, const char *setting_id)
{
    return settings_pack_find(bsp->settings_pack, channel_group_ids[channel_index], setting_id);
}

static bool channel_uses_thermostat(int channel_index)
{
    setting_t *mode = channel_setting(channel_index, MODE_SETTING_ID);
    return mode && mode->oneof.val == CHANNEL_MODE_THERMOSTAT;
}

static int read_temperature_c(void *ctx, float *temperature_c)
{
    struct thermostat_channel_user_ctx *user_ctx = ctx;
    float humidity;

    if (!user_ctx || !user_ctx->local_thermometer || !temperature_c)
        return ESP_ERR_INVALID_ARG;

    if (user_ctx->link_thermometer) {
        double    linked_temperature;
        esp_err_t link_rc = link_thermometer_get_local(user_ctx->link_thermometer,
                                                       &linked_temperature);
        if (link_rc == ESP_OK && isfinite(linked_temperature)) {
            *temperature_c = (float)linked_temperature;
            return ESP_OK;
        }
        if (link_rc == ESP_OK)
            link_rc = ESP_ERR_INVALID_RESPONSE;

        float    local_temperature;
        esp_err_t local_rc = supla_channel_dht_get_local(user_ctx->local_thermometer,
                                                         &humidity, &local_temperature);
        if (local_rc != ESP_OK) {
            ESP_LOGW(TAG, "Linked thermometer unavailable (%s); local fallback failed (%s)",
                     esp_err_to_name(link_rc), esp_err_to_name(local_rc));
            return local_rc;
        }

        ESP_LOGW(TAG, "Linked thermometer unavailable (%s); using local DHT reading",
                 esp_err_to_name(link_rc));
        *temperature_c = local_temperature;
        return link_rc;
    }

    return supla_channel_dht_get_local(user_ctx->local_thermometer, &humidity, temperature_c);
}

static int set_heating(void *ctx, bool enabled)
{
    struct thermostat_channel_user_ctx *user_ctx = ctx;

    if (!user_ctx || !user_ctx->relay)
        return ESP_ERR_INVALID_ARG;
    return enabled ? relay_on(user_ctx->relay) : relay_off(user_ctx->relay);
}

#else
static const settings_group_t board_settings_pack[] = { {} };
#endif

static bsp_t brd_esp32s3_relay_6ch = {
    .id = "ESP32-S3-RELAY-6CH",
    .ver = "1.0",
    .settings_pack = board_settings_pack,
};

bsp_t *const bsp = &brd_esp32s3_relay_6ch;

static supla_status_led_rgb_t status_led;

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
    ESP_ERROR_CHECK(button_init(&btn));
    settings_nvs_read(bsp->settings_pack);
    settings_pack_print(bsp->settings_pack);
#ifdef CONFIG_BSP_RELAY_6CH_ENABLE_HVAC
    for (int i = 0; i < CHANNELS_COUNT; i++) {
        thermometer_captions[i][sizeof(thermometer_captions[i]) - 1] = '\0';
        thermometer_links[i][sizeof(thermometer_links[i]) - 1] = '\0';
    }
#endif
    return ESP_OK;
}

esp_err_t board_supla_init(supla_dev_t *dev)
{
    static const gpio_num_t relay_gpios[CHANNELS_COUNT] = {
        GPIO_NUM_1, GPIO_NUM_2, GPIO_NUM_41, GPIO_NUM_42, GPIO_NUM_45, GPIO_NUM_46,
    };
    struct status_led_rgb_config led_conf = {
        .gpio = GPIO_NUM_38,
        .type = LED_MODEL_WS2812,
        .pixel_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
        .color = { .g = 0xff },
    };

    status_led = supla_status_led_rgb_init(dev, &led_conf);
    if (!status_led)
        return ESP_FAIL;

#ifdef CONFIG_BSP_RELAY_6CH_ENABLE_HVAC
    bool has_thermostat = false;
    for (int i = 0; i < CHANNELS_COUNT; i++)
        has_thermostat = has_thermostat || channel_uses_thermostat(i);

    if (has_thermostat) {
        struct dht_channel_config dht_conf = {
            .sensor_type = DHT_TYPE_AM2301,
            .gpio = GPIO_NUM_9,
        };
        local_temp_channel = supla_channel_dht_create(&dht_conf);
        if (!local_temp_channel) {
            ESP_LOGE(TAG, "Failed to create local thermometer channel");
            return ESP_ERR_NO_MEM;
        }
        if (supla_dev_add_channel(dev, local_temp_channel) != SUPLA_RESULT_TRUE) {
            ESP_LOGE(TAG, "Failed to add local thermometer channel");
            supla_channel_dht_delete(local_temp_channel);
            local_temp_channel = NULL;
            return ESP_FAIL;
        }
    }
#endif

    const struct relay_channel_config relay_channel_conf = {
        .default_function = SUPLA_CHANNELFNC_POWERSWITCH,
        .supported_functions = RELAY_CH_SUPPORTED_FUNC_BITS,
    };

    for (int i = 0; i < CHANNELS_COUNT; i++) {
#ifdef CONFIG_BSP_RELAY_6CH_ENABLE_HVAC
        if (channel_uses_thermostat(i)) {
            struct relay_config relay_conf = {
                .gpio_num = relay_gpios[i],
                .active_high = true,
            };
            struct thermostat_channel_user_ctx *user_ctx = &thermostat_channel_user_ctx[i];
            supla_channel_t *link_channel = NULL;
            supla_channel_t *main_thermometer = local_temp_channel;
            supla_channel_t *aux_thermometer = NULL;

            user_ctx->local_thermometer = local_temp_channel;
            user_ctx->relay = relay_create(&relay_conf);
            if (!user_ctx->relay) {
                ESP_LOGE(TAG, "Failed to create thermostat relay %d", i + 1);
                return ESP_ERR_NO_MEM;
            }

            setting_t *link_setting = channel_setting(i, LINK_SETTING_ID);
            if (link_setting && link_setting->text.val && link_setting->text.val[0]) {
                setting_t *caption_setting = channel_setting(i, CAPTION_SETTING_ID);
                const struct link_thermometer_config link_conf = {
                    .default_caption = caption_setting && caption_setting->text.val[0] ?
                                           caption_setting->text.val :
                                           NULL,
                    .link = link_setting->text.val,
                    .poll_interval_ms = 10000,
                };
                link_channel = link_thermometer_create(&link_conf);
                if (!link_channel) {
                    ESP_LOGE(TAG, "Failed to create link thermometer for channel %d", i + 1);
                    relay_delete(user_ctx->relay);
                    user_ctx->relay = NULL;
                    return ESP_ERR_NO_MEM;
                }
                user_ctx->link_thermometer = link_channel;
                main_thermometer = link_channel;
                aux_thermometer = local_temp_channel;
            }

            struct thermostat_channel_config thermostat_conf = {
                .hal = {
                    .user_ctx = user_ctx,
                    .read_temperature_c = read_temperature_c,
                    .set_heating = set_heating,
                },
                .thermometer_channel = main_thermometer,
                .aux_thermometer_channel = aux_thermometer,
                .aux_thermometer_type =
                    aux_thermometer ? SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED :
                                      SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET,
                .setpoint_c = 22.0f,
                .hysteresis_c = 0.5f,
                .poll_interval_ms = 1000,
            };
            supla_channel_t *thermostat_channel =
                supla_thermostat_channel_create(&thermostat_conf);
            if (!thermostat_channel) {
                ESP_LOGE(TAG, "Failed to create thermostat channel %d", i + 1);
                if (link_channel)
                    link_thermometer_delete(link_channel);
                relay_delete(user_ctx->relay);
                user_ctx->link_thermometer = NULL;
                user_ctx->relay = NULL;
                return ESP_ERR_NO_MEM;
            }

            if (link_channel &&
                supla_dev_add_channel(dev, link_channel) != SUPLA_RESULT_TRUE) {
                ESP_LOGE(TAG, "Failed to add link thermometer for channel %d", i + 1);
                supla_thermostat_channel_delete(thermostat_channel);
                link_thermometer_delete(link_channel);
                relay_delete(user_ctx->relay);
                user_ctx->link_thermometer = NULL;
                user_ctx->relay = NULL;
                return ESP_FAIL;
            }
            if (supla_dev_add_channel(dev, thermostat_channel) != SUPLA_RESULT_TRUE) {
                ESP_LOGE(TAG, "Failed to add thermostat channel %d", i + 1);
                supla_thermostat_channel_delete(thermostat_channel);
                relay_delete(user_ctx->relay);
                user_ctx->relay = NULL;
                return ESP_FAIL;
            }
            continue;
        }
#endif
        struct relay_channel_config channel_conf = relay_channel_conf;
        channel_conf.gpio = relay_gpios[i];

        supla_channel_t *relay_channel = supla_relay_channel_create(&channel_conf);
        if (!relay_channel) {
            ESP_LOGE(TAG, "Failed to create relay channel %d", i + 1);
            return ESP_ERR_NO_MEM;
        }
        if (supla_dev_add_channel(dev, relay_channel) != SUPLA_RESULT_TRUE) {
            ESP_LOGE(TAG, "Failed to add relay channel %d", i + 1);
            supla_relay_channel_delete(relay_channel);
            return ESP_FAIL;
        }
    }

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
