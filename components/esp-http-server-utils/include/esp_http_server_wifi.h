#ifndef _ESP_HTTP_SERVER_WIFI_H_
#define _ESP_HTTP_SERVER_WIFI_H_

#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_httpd_wifi_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

/**@}*/

#endif  // _ESP_HTTP_SERVER_WIFI_H_
