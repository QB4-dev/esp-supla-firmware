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

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "bsp/board.h"
#include "wifi.h"
#include "webserver.h"

static const char *TAG="APP";

static supla_dev_t *supla_dev;

static struct supla_config supla_config = {
    .email = CONFIG_SUPLA_EMAIL,
    .server = CONFIG_SUPLA_SERVER
};

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    /* For accessing reason codes in case of disconnection */
    system_event_info_t *info = &event->event_info;

    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        supla_dev_start(supla_dev);
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

static void on_config_init(void)
{
    char ap_ssid[32];
    supla_esp_generate_hostname(supla_dev,ap_ssid,sizeof(ap_ssid));
    wifi_set_access_point_mode(ap_ssid);
    supla_dev_enter_config_mode(supla_dev);
    webserver_start(&supla_dev);
}

static void on_config_exit(void)
{
    webserver_stop();
    supla_dev_exit_config_mode(supla_dev);
    wifi_set_station_mode();
}

static void supla_dev_state_change_callback(supla_dev_t *dev, supla_dev_state_t state)
{
    supla_log(LOG_INFO,"state -> %s", supla_dev_state_str(state));

    switch(state){
    case SUPLA_DEV_STATE_CONFIG:
        board_config_mode_init();
        break;
    case SUPLA_DEV_STATE_IDLE:
        break;
    case SUPLA_DEV_STATE_INIT:
      break;
    case SUPLA_DEV_STATE_CONNECTED:
      break;
    case SUPLA_DEV_STATE_REGISTERED:
      break;
    case SUPLA_DEV_STATE_ONLINE:
      break;
    default:
      break;
    }
}

static void supla_task(void *arg)
{
    supla_dev_t *dev = arg;
    if(!dev){
        vTaskDelete(NULL);
        return;
    }

    if(supla_dev_set_config(dev,&supla_config) != SUPLA_RESULT_TRUE){
        vTaskDelete(NULL);
        return;
    }

    while(1){
        supla_dev_iterate(dev);
        vTaskDelay(100 / portTICK_RATE_MS);
    }
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(wifi_init(event_handler));
    ESP_ERROR_CHECK(supla_esp_nvs_config_init(&supla_config));

    supla_dev = supla_dev_create("SUPLA-DEV",NULL);
    board_set_config_init_callback(on_config_init);
    board_set_config_exit_callback(on_config_exit);
    ESP_ERROR_CHECK(board_init(supla_dev));

    supla_dev_set_name(supla_dev,bsp->id);
    supla_dev_set_flags(supla_dev,SUPLA_DEVICE_FLAG_CALCFG_ENTER_CFG_MODE);
    supla_dev_set_state_changed_callback(supla_dev,supla_dev_state_change_callback);
    supla_dev_set_common_channel_state_callback(supla_dev,supla_esp_get_wifi_state);
    supla_dev_set_server_time_sync_callback(supla_dev,supla_esp_server_time_sync);
    supla_dev_set_config(supla_dev,&supla_config);

    xTaskCreate(&supla_task, "supla", 8192, supla_dev, 1, NULL);
    wifi_set_station_mode();
}
