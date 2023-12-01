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

static const char *TAG="HTTPD";

static httpd_handle_t server = NULL;

#define DECLARE_EMBED_HANDLER(NAME, URI, CT) \
    extern const char embed_##NAME[] asm("_binary_"#NAME"_start"); \
    extern const char size_##NAME[] asm("_binary_"#NAME"_size"); \
    esp_err_t get_##NAME(httpd_req_t *req) { \
        httpd_resp_set_type(req, CT); \
        return httpd_resp_send(req, embed_##NAME,(size_t)&size_##NAME); \
    } \
    static const httpd_uri_t route_get_##NAME = { .uri = (URI), .method = HTTP_GET, .handler = get_##NAME }

/// Handlers

DECLARE_EMBED_HANDLER(index_html, "/index.html", "text/html");
//DECLARE_EMBED_HANDLER(supla_css, "/supla.css", "text/css");
static const httpd_uri_t route_get_root = { .uri = "/", .method = HTTP_GET, .handler = get_index_html };

static httpd_uri_t wifi_get_handler = { .uri = "/wifi", .method  = HTTP_GET, .handler = esp_httpd_wifi_handler };
static httpd_uri_t wifi_post_handler = { .uri = "/wifi", .method  = HTTP_POST, .handler = esp_httpd_wifi_handler };

static httpd_uri_t supla_get_handler = { .uri = "/supla", .method  = HTTP_GET, .handler = supla_dev_httpd_handler };
static httpd_uri_t supla_post_handler = { .uri = "/supla", .method  = HTTP_POST, .handler = supla_dev_httpd_handler };

static httpd_uri_t info_handler = { .uri = "/info", .method  = HTTP_GET, .handler = esp_httpd_app_info_handler };
static httpd_uri_t fota_handler = { .uri = "/update", .method  = HTTP_POST, .handler = esp_httpd_fota_handler };

static esp_err_t init(supla_dev_t **dev)
{
    // Static file handlers
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_index_html));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_root));

    // Additional handlers
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_get_handler));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wifi_post_handler));

    supla_get_handler.user_ctx = dev;
    supla_post_handler.user_ctx = dev;
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &supla_get_handler));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &supla_post_handler));

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &info_handler));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &fota_handler));
    return ESP_OK;
}

/// Public

esp_err_t webserver_init(supla_dev_t **dev)
{
    if (server){
        ESP_LOGI(TAG, "HTTPD started, trying to stop...");
        httpd_stop(server);
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 100;
    config.lru_purge_enable = true;

    ESP_ERROR_CHECK(httpd_start(&server, &config));
    ESP_ERROR_CHECK(init(dev));

    ESP_LOGI(TAG, "server started on port %d, free mem: %d bytes", config.server_port, esp_get_free_heap_size());
    return ESP_OK;
}
