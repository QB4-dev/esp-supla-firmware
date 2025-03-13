#ifndef SETTINGS_H_
#define SETTINGS_H_

#include <esp_http_server.h>
#include <esp_err.h>

#include "settings-defs.h"

typedef esp_err_t (*settings_handler_t)(const settings_group_t *settings, void *arg);

void       settings_pack_print(const settings_group_t *settings);
setting_t *settings_pack_find(const settings_group_t *settings, const char *gr, const char *id);

void setting_set_defaults(setting_t *setting);
void settings_pack_set_defaults(const settings_group_t *settings_pack);

esp_err_t settings_nvs_read(const settings_group_t *settings);
esp_err_t settings_nvs_write(const settings_group_t *settings);
esp_err_t settings_nvs_erase(void);

esp_err_t settings_handler_register(settings_handler_t handler, void *arg);
esp_err_t settings_httpd_handler(httpd_req_t *req);

#endif /* SETTINGS_H_ */
