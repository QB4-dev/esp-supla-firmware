/* Simple WAV playback example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_system.h>
#include <esp_log.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <nvs_flash.h>
#include <esp_event_loop.h>
#include <tcpip_adapter.h>

#include <driver/gpio.h>

#include <esp-supla.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string.h>

#include "webserver.h"

#include <ledc-channel.h>

static const char *TAG="APP";

static struct supla_config supla_config = {
    .email = CONFIG_SUPLA_EMAIL,
    .server = CONFIG_SUPLA_SERVER
};

static supla_dev_t *supla_dev;
static supla_channel_t *relay_channel;

#define LED_PIN GPIO_NUM_2

//RELAY
int led_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    TRelayChannel_Value *relay_val = (TRelayChannel_Value*)new_value->value;

    supla_log(LOG_INFO,"Relay set value %d",relay_val->hi);
    gpio_set_level(LED_PIN,!relay_val->hi);
    return supla_channel_set_relay_value(ch,relay_val);
}

static supla_channel_config_t relay_channel_config = {
    .type = SUPLA_CHANNELTYPE_RELAY,
    .supported_functions = 0xFF,
    .default_function = SUPLA_CHANNELFNC_LIGHTSWITCH,
    .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
    .on_set_value = led_set_value
};



static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    /* For accessing reason codes in case of disconnection */
    system_event_info_t *info = &event->event_info;

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        ESP_LOGE(TAG, "Disconnect reason : %d", info->disconnected.reason);
        if (info->disconnected.reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
            /*Switch to 802.11 bgn mode */
            esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        }
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
        ESP_LOGI(TAG, "assigned ip:%s", ip4addr_ntoa(&event->event_info.ap_staipassigned.ip));
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_setup(void)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config_ap = {
        .ap = {
            .ssid = "esp8266",
            .ssid_len = strlen("esp8266"),
            .password = "",
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    wifi_config_t wifi_config_sta = {
        .sta = {
            .ssid = "Dom",
            .password = "12345678"
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    //ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config_ap));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_sta));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void supla_task(void *arg)
{
    supla_dev_t *dev = arg;
    if(!dev){
        vTaskDelete(NULL);
        return;
    }

    supla_channel_t *ledc_channel = supla_ledc_channel_create(GPIO_NUM_3,1000);
    supla_dev_add_channel(dev,ledc_channel);

    supla_dev_set_common_channel_state_callback(dev,supla_esp_get_wifi_state);
    supla_dev_set_server_time_sync_callback(dev,supla_esp_server_time_sync);

    if(supla_dev_set_config(dev,&supla_config) != SUPLA_RESULT_TRUE){
        vTaskDelete(NULL);
        return;
    }

    supla_dev_start(dev);
    while(1){
        supla_dev_iterate(dev);
        vTaskDelay(100 / portTICK_RATE_MS);
    }
}


void app_main()
{
    ESP_LOGI(TAG, "server gen demo");
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(supla_esp_nvs_config_init(&supla_config));
    supla_dev = supla_dev_create("ESP-mini",NULL);

    wifi_setup();
    webserver_init();

    xTaskCreate(&supla_task, "supla", 8192, supla_dev, 1, NULL);
    for (;;) {
        ESP_LOGI(TAG, "Updated free heap size: '%d'", esp_get_free_heap_size());
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
}
