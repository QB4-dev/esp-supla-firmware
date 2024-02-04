/*
 * settings.h
 *
 *  Created on: 3 lut 2024
 *      Author: kuba
 */

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <stdbool.h>
#include <inttypes.h>

#include <esp_http_server.h>
#include <esp_err.h>

typedef enum {
    SETTING_TYPE_BOOL = 0,
    SETTING_TYPE_NUM = 1,
    SETTING_TYPE_ONEOF = 2,
} setting_type_t;

typedef struct {
    bool val;
    bool def;
} setting_bool_t;

typedef struct {
    int val;
    int def;
    int range[2];
} setting_int_t;

typedef struct {
    int val;
    int def;
    const char **labels;
} setting_oneof_t;

typedef struct {
    const char *label;
    setting_type_t type;
    bool disabled;
    union {
        setting_bool_t boolean;
        setting_int_t num;
        setting_oneof_t oneof;
    };
} setting_t;

typedef struct {
    const char *label;
    setting_t *settings;
} settings_group_t;

void settings_pack_print(const settings_group_t *group);
setting_t *settings_pack_find(const settings_group_t *settings_pack, const char *group, const char *label);

esp_err_t settings_nvs_read(const settings_group_t *settings_pack);
esp_err_t settings_nvs_write(settings_group_t *settings_pack);
esp_err_t settings_nvs_erase(void);

esp_err_t settings_httpd_handler(httpd_req_t *req);

#endif /* SETTINGS_H_ */
