/*
 * settings.c
 *
 *  Created on: 3 lut 2024
 *      Author: kuba
 */


#include "include/settings.h"

#include <stdio.h>
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <cJSON.h>

static const char *TAG = "SETTINGS";
static const char *NVS_STORAGE = "settings_nvs";

void settings_pack_print(const settings_group_t *settings_pack)
{
    for(const settings_group_t *gr = settings_pack; gr->label; gr++){
        printf("gr %s\n",gr->label);
        for(setting_t *setting = gr->settings; setting->label; setting++){
            printf("- %s: ",setting->label);
            switch (setting->type){
                case SETTING_TYPE_BOOL:
                    printf("%s\n",setting->boolean.val ? "ENABLED":"DISABLED");
                    break;
                case SETTING_TYPE_NUM:
                    printf("%d\n",setting->num.val);
                    break;
                case SETTING_TYPE_ONEOF:
                    printf("%s\n",setting->oneof.labels[setting->oneof.val]);
                    break;
                default:
                    break;
            }
        }
    }
}

setting_t *settings_pack_find(const settings_group_t *settings_pack, const char *group, const char *label)
{
    for(const settings_group_t *gr = settings_pack; gr->label; gr++){
        if(strcmp(group,gr->label))
            continue;
        for(setting_t *setting = gr->settings; setting->label; setting++){
            if(!strcmp(label,setting->label))
                return setting;
        }
    }
    return NULL;
}

esp_err_t settings_nvs_read(const settings_group_t *settings_pack)
{
    char nvs_label[128];
    nvs_handle nvs;
    esp_err_t rc;

    nvs_flash_init();
    ESP_LOGI(TAG,"NVS init");
    rc = nvs_open(NVS_STORAGE,NVS_READONLY,&nvs);
    if(rc == ESP_OK){
        for(const settings_group_t *gr = settings_pack; gr->label; gr++){
            for(setting_t *setting = gr->settings; setting->label; setting++){
                sprintf(nvs_label,"%s:%s",gr->label,setting->label);
                switch (setting->type){
                    case SETTING_TYPE_BOOL:{
                        nvs_get_i8(nvs,nvs_label,(int8_t*)&setting->boolean.val);
                        }break;
                    case SETTING_TYPE_NUM:{
                        nvs_get_i32(nvs,nvs_label,&setting->num.val);
                        }break;
                    case SETTING_TYPE_ONEOF:{
                        nvs_get_i8(nvs,nvs_label,(int8_t*)&setting->oneof.val);
                        }break;
                    default:
                        break;
                }
            }
        }
        nvs_close(nvs);
    } else {
        ESP_LOGW(TAG, "nvs open error %s",esp_err_to_name(rc));
    }
    return ESP_OK;
}

esp_err_t settings_nvs_write(settings_group_t *settings_pack)
{
    char nvs_label[128];
    nvs_handle nvs;
    esp_err_t rc;

    rc = nvs_open(NVS_STORAGE,NVS_READWRITE,&nvs);
    if(rc == ESP_OK){
        for(settings_group_t *gr = settings_pack; gr->label; gr++){
            for(setting_t *setting = gr->settings; setting->label; setting++){
                sprintf(nvs_label,"%s:%s",gr->label,setting->label);
                switch (setting->type){
                    case SETTING_TYPE_BOOL:
                        rc = nvs_set_i8(nvs,nvs_label,setting->boolean.val);
                        break;
                    case SETTING_TYPE_NUM:
                        rc = nvs_set_i32(nvs,nvs_label,setting->num.val);
                        break;
                    case SETTING_TYPE_ONEOF:
                        rc = nvs_set_i8(nvs,nvs_label,setting->oneof.val);
                        break;
                    default:
                        break;
                }
                if(rc != ESP_OK)
                    ESP_LOGE(TAG, "nvs set: %s",esp_err_to_name(rc));
            }
        }
        nvs_commit(nvs);
        nvs_close(nvs);
    } else {
        ESP_LOGE(TAG, "nvs open error %s",esp_err_to_name(rc));
        return rc;
    }
    return ESP_OK;
}

esp_err_t settings_nvs_erase(void)
{
    nvs_handle nvs;
    esp_err_t rc;
    rc = nvs_open(NVS_STORAGE,NVS_READWRITE,&nvs);
    if(rc == ESP_OK){
        nvs_erase_all(nvs);
        ESP_LOGW(TAG, "nvs erased");
    }else{
        ESP_LOGE(TAG, "nvs open error %s",esp_err_to_name(rc));
    }
    return rc;
}

static esp_err_t send_json_response(cJSON *js, httpd_req_t *req)
{
    char *js_txt = cJSON_Print(js);
    cJSON_Delete(js);

    httpd_resp_set_type(req,HTTPD_TYPE_JSON);
    httpd_resp_send(req, js_txt, -1);
    free(js_txt);
    return ESP_OK;
}

static cJSON *settings_pack_to_json(settings_group_t *settings_pack)
{
    cJSON *js_groups;
    cJSON *js_group;
    cJSON *js_settings;
    cJSON *js_setting;
    cJSON *js_labels;
    cJSON *js;

    const char *types[] = {
       [SETTING_TYPE_BOOL] = "BOOL",
       [SETTING_TYPE_NUM] = "NUM",
       [SETTING_TYPE_ONEOF] = "ONEOF",
    };

    if(!settings_pack)
        return NULL;

    js_groups = cJSON_CreateArray();
    for(settings_group_t *gr = settings_pack; gr->label; gr++){
        js_group = cJSON_CreateObject();
        cJSON_AddStringToObject(js_group,"label",gr->label);
        js_settings = cJSON_CreateArray();
        for(setting_t *setting = gr->settings; setting->label; setting++){
            js_setting = cJSON_CreateObject();
            cJSON_AddStringToObject(js_setting,"label",setting->label);
            cJSON_AddStringToObject(js_setting,"type",types[setting->type]);
            switch (setting->type){
                case SETTING_TYPE_BOOL:
                    cJSON_AddBoolToObject(js_setting,"val",setting->boolean.val);
                    cJSON_AddBoolToObject(js_setting,"def",setting->boolean.def);
                    break;
                case SETTING_TYPE_NUM:
                    cJSON_AddNumberToObject(js_setting,"val",setting->num.val);
                    cJSON_AddNumberToObject(js_setting,"def",setting->num.def);
                    cJSON_AddNumberToObject(js_setting,"min",setting->num.range[0]);
                    cJSON_AddNumberToObject(js_setting,"max",setting->num.range[1]);
                    break;
                case SETTING_TYPE_ONEOF:
                    cJSON_AddNumberToObject(js_setting,"val",setting->oneof.val);
                    cJSON_AddNumberToObject(js_setting,"def",setting->oneof.def);
                    js_labels = cJSON_CreateArray();
                    cJSON_AddItemToObject(js_setting,"labels",js_labels);
                    for(const char **label = setting->oneof.labels;*label != NULL ;label++)
                        cJSON_AddItemToArray(js_labels,cJSON_CreateString(*label));
                    break;
                default:
                    break;
            }
            cJSON_AddItemToArray(js_settings,js_setting);
        }
        cJSON_AddItemToObject(js_group,"settings",js_settings);
        cJSON_AddItemToArray(js_groups,js_group);
    }
    js = cJSON_CreateObject();
    cJSON_AddItemToObject(js,"groups",js_groups);
    return js;
}

static esp_err_t set_req_handle(httpd_req_t *req)
{
    char *req_data;
    char srch_label[128];
    char value[128];
    int bytes_recv = 0;
    int rc;

    settings_group_t *settings_pack = req->user_ctx;
    if(req->content_len){
        req_data = calloc(1,req->content_len+1);
        if(!req_data)
            return ESP_ERR_NO_MEM;

        for(int bytes_left=req->content_len; bytes_left > 0; ) {
            if ((rc = httpd_req_recv(req, req_data+bytes_recv, bytes_left)) <= 0) {
                if (rc == HTTPD_SOCK_ERR_TIMEOUT)
                    continue;
                else
                    return ESP_FAIL;
            }
            bytes_recv += rc;
            bytes_left -= rc;
        }

        for(settings_group_t *gr = settings_pack; gr->label; gr++){
            for(setting_t *setting = gr->settings; setting->label; setting++){
                sprintf(srch_label,"%s:%s",gr->label,setting->label);
                if(httpd_query_key_value(req_data,srch_label,value,sizeof(value)) != ESP_OK){
                    if(setting->type == SETTING_TYPE_BOOL)
                        setting->boolean.val = false;
                    continue;
                }

                switch (setting->type){
                    case SETTING_TYPE_BOOL:{
                        setting->boolean.val = !strcmp("on",value);
                        }break;
                    case SETTING_TYPE_NUM:{
                        int num_val = atoi(value);
                        if(num_val >= setting->num.range[0] && num_val <= setting->num.range[1])
                            setting->num.val = atoi(value);
                        }break;
                    case SETTING_TYPE_ONEOF:{
                        int num_val = atoi(value);
                        int labels_count = 0;
                        for(const char **label = setting->oneof.labels;*label != NULL ;label++)
                            labels_count++;
                        if(num_val < labels_count)
                            setting->oneof.val = atoi(value);
                        }break;
                    default:
                        break;
                }
            }
        }
        free(req_data);
    }

    rc = settings_nvs_write(settings_pack);
    if(rc == 0){
        ESP_LOGI(TAG,"nvs write OK");
        return ESP_OK;
    }else{
        ESP_LOGE(TAG,"nvs write ERR:%s(%d)", esp_err_to_name(rc),rc);
        return rc;
    }
}

esp_err_t settings_httpd_handler(httpd_req_t *req)
{
    cJSON *js;
    char *url_query;
    size_t qlen;
    char value[128];

    js = cJSON_CreateObject();

    settings_group_t *settings_pack = req->user_ctx;

    //parse URL query
    qlen = httpd_req_get_url_query_len(req)+1;
    if (qlen > 1) {
        url_query = malloc(qlen);
        if (httpd_req_get_url_query_str(req, url_query, qlen) == ESP_OK) {
            if (httpd_query_key_value(url_query, "action", value, sizeof(value)) == ESP_OK) {
                if(!strcmp(value,"set")){
                    set_req_handle(req);
                }if(!strcmp(value,"erase")){
                    settings_nvs_erase();
                }
            }
        }
        free(url_query);
    }
    cJSON_AddItemToObject(js,"data",settings_pack_to_json(settings_pack));
    return send_json_response(js,req);
}

