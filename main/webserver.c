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
static const httpd_uri_t route_get_root = { .uri = "/", .method = HTTP_GET, .handler = get_index_html };

static httpd_uri_t info_handler = { .uri = "/info", .method  = HTTP_GET, .handler = app_info_handler };
static httpd_uri_t fota_handler = { .uri = "/update", .method  = HTTP_POST, .handler = esp_httpd_fota_handler };

static esp_err_t init()
{
    // API handlers
    //CHECK(api_init(server));

    // Static file handlers
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_index_html));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route_get_root));
    // Additional handlers
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &info_handler));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &fota_handler));
    return ESP_OK;
}

/// Public

esp_err_t webserver_init()
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
    ESP_ERROR_CHECK(init());

    ESP_LOGI(TAG, "HTTPD started on port %d, free mem: %d bytes", config.server_port, esp_get_free_heap_size());

    return ESP_OK;
}
