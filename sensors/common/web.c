/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@proton.me>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <esp_log.h>

#include <esp_wifi.h>
#include <esp_transport.h>
#include <esp_transport_tcp.h>
#include <esp_transport_ssl.h>
#include <esp_transport_ws.h>

#include <esp_http_server.h>

#include "settings.h"

#define TAG "web"

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

#define MAX_WS_CLIENTS 4
static int ws_clients[MAX_WS_CLIENTS] = {0};
static httpd_handle_t server;

void web_broadcast(const char *msg)
{
    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)msg,
        .len = strlen(msg),
    };

    for(int i=0; i<MAX_WS_CLIENTS; i++) {
        int fd = ws_clients[i];
        if(!fd)
            continue;

        esp_err_t err = httpd_ws_send_frame_async(server, fd, &frame);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "send to fd=%d failed: %s", fd, esp_err_to_name(err));
            ws_clients[i] = 0;
        }
    }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    int fd = httpd_req_to_sockfd(req);
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "websocket client");
        for(int i=0; i<MAX_WS_CLIENTS; i++)
            if(ws_clients[i] == fd)
                return ESP_OK;
        
        for(int i=0; i<MAX_WS_CLIENTS; i++) {
            if(ws_clients[i] == 0) {
                ws_clients[i] = fd;
                return ESP_OK;
            }
        }
        httpd_resp_send_err(req, HTTPD_405_METHOD_NOT_ALLOWED, "Too many websocket clients");
        return ESP_FAIL;
    }
    return ESP_OK; // do nothing
}

static esp_err_t submit_handler(httpd_req_t *req)
{
    esp_err_t err;
    char *buf = NULL;
    char channel_str[8] = "";
    char rate_str[8] = "";
    char tx_power_str[8] = "";
    char psk[64] = "";

    if (req->content_len <= 0 || req->content_len > 256) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad form size");
        return ESP_FAIL;
    }

    buf = malloc(req->content_len + 1);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return ESP_FAIL;
    }

    int len = httpd_req_recv(req, buf, req->content_len);
    if (len <= 0) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive form");
        return ESP_FAIL;
    }
    buf[len] = 0;

    httpd_query_key_value(buf, "channel", channel_str, sizeof(channel_str));
    httpd_query_key_value(buf, "rate", rate_str, sizeof(rate_str));
    httpd_query_key_value(buf, "tx_power", tx_power_str, sizeof(tx_power_str));

    if (httpd_query_key_value(buf, "psk", psk, sizeof(psk)) != ESP_OK) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing PSK");
        return ESP_FAIL;
    }

    free(buf);

    int channel = atoi(channel_str);
    int rate = atoi(rate_str);
    int tx_power = atoi(tx_power_str);

    if (!(channel == 1 || channel == 6 || channel == 11)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid channel");
        return ESP_FAIL;
    }

    if (!(rate == 1 || rate == 2 || rate == 5 || rate == 10)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid rate");
        return ESP_FAIL;
    }

    if (!(tx_power == 8 || tx_power == 13 || tx_power == 16 || tx_power == 20)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid tx_power");
        return ESP_FAIL;
    }

    err = write_settings(channel, rate, tx_power, psk);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save config");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req,
        "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<meta http-equiv='refresh' content='3;url=/'>"
        "<style>"
        "body{font-family:Arial,sans-serif;background:#f4f6f8;text-align:center;padding-top:40px;}"
        ".card{display:inline-block;background:white;padding:24px 32px;border-radius:10px;"
        "box-shadow:0 4px 12px rgba(0,0,0,0.1);}"
        "</style>"
        "</head><body>"
        "<div class='card'>"
        "<h2>Settings saved</h2>"
        "<p>Rebooting device...</p>"
        "<script>setTimeout(()=>{window.location='/'},3000);</script>"
        "</div>"
        "</body></html>");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

static esp_err_t config_handler(httpd_req_t *req)
{
    char config[1024];
    snprintf(config, sizeof config, "{\"channel\":%d,\"rate\":%d,\"tx_power\":%d,\"psk\":\"%s\"}",
             wifi_channel, rate, tx_power, wifi_psk);
    
    httpd_resp_send(req, (const char *) config, strlen(config));
    return ESP_OK;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_send(req, (const char *) index_html_start, index_html_end - index_html_start);
	return ESP_OK;
}

void web_stop() {
    httpd_stop(server);
}

void web_setup() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //config.max_open_sockets = 13;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;
    ESP_LOGI(TAG, "Starting server on port %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server");
        return;
    }
    
    ESP_LOGI(TAG, "Server started successfully, registering URI handlers...");

    static const httpd_uri_t ws = {
        .uri       = "/ws",
        .method    = (httpd_method_t)HTTP_GET,
        .handler   = ws_handler,
        .user_ctx  = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server, &ws);

    static const httpd_uri_t submit = {
        .uri       = "/submit",
        .method    = (httpd_method_t)HTTP_POST,
        .handler   = submit_handler,
        .user_ctx  = NULL,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server, &submit);
    {
    static const httpd_uri_t config = {
        .uri       = "/config",
        .method    = (httpd_method_t)HTTP_GET,
        .handler   = config_handler,
        .user_ctx  = NULL,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server, &config);
    }
    static const httpd_uri_t root = {
        .uri       = "/*",
        .method    = (httpd_method_t)HTTP_GET,
        .handler   = root_handler,
        .user_ctx  = NULL,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server, &root);
}
