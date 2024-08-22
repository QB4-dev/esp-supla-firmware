/*
 * webserver.c
 *
 *  Created on: 28 lis 2023
 *      Author: kuba
 */

#include "webserver.h"
#include <esp_log.h>
#include <esp_system.h>
#include <esp_http_server.h>
#include <esp_http_server_fota.h>
#include <esp_http_server_wifi.h>

static const char *TAG = "HTTPD";

static httpd_handle_t server = NULL;

#if CONFIG_IDF_TARGET_ESP8266
#define DECLARE_EMBED_HANDLER(NAME, URI, CT)                               \
    extern const char embed_##NAME[] asm("_binary_" #NAME "_start");       \
    extern const char size_##NAME[] asm("_binary_" #NAME "_size");         \
    esp_err_t         get_##NAME(httpd_req_t *req)                         \
    {                                                                      \
        httpd_resp_set_type(req, CT);                                      \
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");               \
        return httpd_resp_send(req, embed_##NAME, (size_t) & size_##NAME); \
    }                                                                      \
    static const httpd_uri_t route_get_##NAME = { .uri = (URI),            \
                                                  .method = HTTP_GET,      \
                                                  .handler = get_##NAME }
#elif CONFIG_IDF_TARGET_ESP32
#define DECLARE_EMBED_HANDLER(NAME, URI, CT)                          \
    extern const char embed_##NAME[] asm("_binary_" #NAME "_start");  \
    extern const char end_##NAME[] asm("_binary_" #NAME "_end");      \
    esp_err_t         get_##NAME(httpd_req_t *req)                    \
    {                                                                 \
        size_t size = sizeof(uint8_t) * (end_##NAME - embed_##NAME);  \
        httpd_resp_set_type(req, CT);                                 \
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");          \
        return httpd_resp_send(req, embed_##NAME, size);              \
    }                                                                 \
    static const httpd_uri_t route_get_##NAME = { .uri = (URI),       \
                                                  .method = HTTP_GET, \
                                                  .handler = get_##NAME }
#endif

#define DECLARE_EMBED_HANDLER_RAW(NAME, URI, CT)                           \
    extern const char embed_##NAME[] asm("_binary_" #NAME "_start");       \
    extern const char size_##NAME[] asm("_binary_" #NAME "_size");         \
    esp_err_t         get_##NAME(httpd_req_t *req)                         \
    {                                                                      \
        httpd_resp_set_type(req, CT);                                      \
        return httpd_resp_send(req, embed_##NAME, (size_t) & size_##NAME); \
    }                                                                      \
    static const httpd_uri_t route_get_##NAME = { .uri = (URI),            \
                                                  .method = HTTP_GET,      \
                                                  .handler = get_##NAME }

DECLARE_EMBED_HANDLER(index_html_gz, "/index.html", "text/html");
DECLARE_EMBED_HANDLER(supla_css_gz, "/supla.css", "text/css");
DECLARE_EMBED_HANDLER(script_js_gz, "/script.js", "text/javascript");

//DECLARE_EMBED_HANDLER_RAW(config_html, "/", "text/html");

static const httpd_uri_t route_get_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_index_html_gz //default
};

static httpd_uri_t wifi_get_handler = {
    .uri = "/wifi",
    .method = HTTP_GET,
    .handler = esp_httpd_wifi_handler //
};
static httpd_uri_t wifi_post_handler = {
    .uri = "/wifi",
    .method = HTTP_POST,
    .handler = esp_httpd_wifi_handler //
};

static httpd_uri_t supla_get_handler = {
    .uri = "/supla",
    .method = HTTP_GET,
    .handler = supla_dev_httpd_handler //
};
static httpd_uri_t supla_post_handler = {
    .uri = "/supla",
    .method = HTTP_POST,
    .handler = supla_dev_httpd_handler //
};

static httpd_uri_t info_handler = {
    .uri = "/info",
    .method = HTTP_GET,
    .handler = esp_httpd_app_info_handler //
};
static httpd_uri_t fota_handler = {
    .uri = "/update",
    .method = HTTP_POST,
    .handler = esp_httpd_fota_handler //
};

static httpd_uri_t settings_get_handler = {
    .uri = "/settings",
    .method = HTTP_GET,
    .handler = settings_httpd_handler //
};
static httpd_uri_t settings_post_handler = {
    .uri = "/settings",
    .method = HTTP_POST,
    .handler = settings_httpd_handler //
};

//static httpd_uri_t basic_get_handler = {
//    .uri = "/",
//    .method = HTTP_GET,
//    .handler = supla_dev_basic_httpd_handler //
//};
//
//static httpd_uri_t basic_post_handler = {
//    .uri = "/",
//    .method = HTTP_POST,
//    .handler = supla_dev_basic_httpd_handler //
//};

static esp_err_t init(supla_dev_t **dev, const settings_group_t *settings_pack)
{
    // Static file handlers
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_index_html_gz));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_script_js_gz));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_supla_css_gz));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_root));

    // Additional handlers
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_get_handler));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_post_handler));

    supla_get_handler.user_ctx = dev;
    supla_post_handler.user_ctx = dev;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &supla_get_handler));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &supla_post_handler));

    //    basic_get_handler.user_ctx = dev;
    //    basic_post_handler.user_ctx = dev;

    //ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_config_html));
    //    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &basic_get_handler));
    //    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &basic_post_handler));

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &info_handler));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &fota_handler));

    if (settings_pack != NULL) {
        settings_get_handler.user_ctx = (void *)settings_pack;
        settings_post_handler.user_ctx = (void *)settings_pack;
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &settings_get_handler));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &settings_post_handler));
    }
    return ESP_OK;
}

esp_err_t webserver_start(supla_dev_t **dev, const settings_group_t *settings_pack)
{
    if (server) {
        ESP_LOGI(TAG, "server started, trying to stop...");
        httpd_stop(server);
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.lru_purge_enable = true;
    config.stack_size = 4 * 4096;

    ESP_ERROR_CHECK(httpd_start(&server, &config));
    ESP_ERROR_CHECK(init(dev, settings_pack));

    ESP_LOGI(TAG, "server started on port %d, free mem: %" PRIu32 " bytes", config.server_port,
             esp_get_free_heap_size());
    return ESP_OK;
}

esp_err_t webserver_stop(void)
{
    esp_err_t rc;
    if (server) {
        ESP_LOGI(TAG, "server stop...");
        rc = httpd_stop(server);
        server = NULL;
        return rc;
    }
    return ESP_ERR_NOT_FOUND;
}
