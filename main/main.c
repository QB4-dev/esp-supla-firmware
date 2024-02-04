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

#include "device.h"
#include "bsp/board.h"
#include "wifi.h"
#include "webserver.h"

static const char *TAG="APP";

static supla_dev_t *supla_dev;
static char ap_ssid[32];

static struct supla_config supla_config = {
    .email = CONFIG_SUPLA_EMAIL,
    .server = CONFIG_SUPLA_SERVER
};

static void net_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch(event_id) {
        case WIFI_EVENT_STA_START:
            //esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_CONNECTED:{
            wifi_event_sta_connected_t *info = event_data;

            ESP_LOGI(TAG, "Connected to SSID: %s",info->ssid);
            }break;
        case WIFI_EVENT_STA_DISCONNECTED:{
            wifi_event_sta_disconnected_t *info = event_data;

            ESP_LOGE(TAG, "Station disconnected(reason : %d)",info->reason);
//          if (info->reason == WIFI_REASON_BASIC_RATE_NOT_SUPPORT) {
//              /*Switch to 802.11 bgn mode */
//              esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
//          }
            if (info->reason == WIFI_REASON_ASSOC_LEAVE) {
                /* disconnected by user */
                break;
            }
            esp_wifi_connect();
            }break;
        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        switch(event_id){
        case IP_EVENT_STA_GOT_IP:{
            ip_event_got_ip_t* event = event_data;
            ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->ip_info.ip));
            supla_dev_start(supla_dev);
            }break;
        default:
            break;
        }
    }
}

static void dev_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
    case DEVICE_EVENT_CONFIG_INIT:
        board_on_config_mode_init();
        wifi_set_access_point_mode(ap_ssid);
        supla_dev_enter_config_mode(supla_dev);
        webserver_start(&supla_dev);
        break;
    case DEVICE_EVENT_CONFIG_EXIT:{
        board_on_config_mode_exit();
        webserver_stop();
        supla_dev_exit_config_mode(supla_dev);
        wifi_set_station_mode();
        }break;
    default:
        break;
    }
}

static void supla_dev_state_change_callback(supla_dev_t *dev, supla_dev_state_t state)
{
    supla_log(LOG_INFO,"state -> %s", supla_dev_state_str(state));

    switch(state){
    case SUPLA_DEV_STATE_CONFIG:
        device_init_config();
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

static esp_err_t supla_init(void)
{
    supla_esp_nvs_config_init(&supla_config);
    supla_dev = supla_dev_create(bsp->id,NULL);
    supla_dev_set_flags(supla_dev,SUPLA_DEVICE_FLAG_CALCFG_ENTER_CFG_MODE);
    supla_dev_set_state_changed_callback(supla_dev,supla_dev_state_change_callback);
    supla_dev_set_common_channel_state_callback(supla_dev,supla_esp_get_wifi_state);
    supla_dev_set_server_time_sync_callback(supla_dev,supla_esp_server_time_sync);
    supla_dev_set_config(supla_dev,&supla_config);
    supla_esp_generate_hostname(supla_dev,ap_ssid,sizeof(ap_ssid));
    return ESP_OK;
}

static void supla_task(void *arg)
{
    supla_dev_t *dev = arg;

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
    ESP_ERROR_CHECK(board_early_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(device_init(dev_event_handler,NULL));
    ESP_ERROR_CHECK(wifi_init(net_event_handler));

    ESP_ERROR_CHECK(supla_init());
    ESP_ERROR_CHECK(board_init(supla_dev));

    ESP_ERROR_CHECK(webserver_use_settings(bsp->settings_pack));

    xTaskCreate(&supla_task, "supla", 8192, supla_dev, 1, NULL);
    //webserver_start(&supla_dev);//REMOVE LATER
    wifi_set_station_mode();
}
