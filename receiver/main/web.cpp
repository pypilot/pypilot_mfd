/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@proton.me>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <unordered_set>

#include <esp_log.h>

#include <esp_wifi.h>
#include <esp_transport.h>
#include <esp_transport_tcp.h>
#include <esp_transport_ssl.h>
#include <esp_transport_ws.h>
#include <esp_ota_ops.h>
#include <esp_http_server.h>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "settings.h"
#include "display.h"
#include "sensors.h"
#include "wireless.h"
#include "data.h"
#include "utils.h"
#include "zeroconf.h"

// TODO: fix these to put in separate header or file
void sensors_write_transmitters(rapidjson::Writer<rapidjson::StringBuffer> &writer, bool info);
void sensors_read_transmitters(const rapidjson::Value &t);
void sensors_wireless_setting(const rapidjson::Document &d);

#define TAG "web"

static httpd_handle_t server;
static std::unordered_set<int> ws_clients, ws_data_clients;

static std::string jsonCurrent()
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    writer.SetMaxDecimalPlaces(2);
    writer.StartObject(); {
        writer.Key("wind");
        writer.StartObject(); {
            writer.Key("direction");
            writer.Double(lpwind_dir);
            writer.Key("knots");
            writer.Double(wind_knots);
        }
        writer.EndObject();
    }
    writer.EndObject();

    return buffer.GetString();
}

static std::string jsonDisplayData() {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    writer.SetMaxDecimalPlaces(2);
    float value;
    std::string source;
    uint64_t time, t0 = esp_timer_get_time();
    writer.StartObject();
    for(int i=0; i<DISPLAY_COUNT; i++)
        if(display_data_get((display_item_e)i, value, source, time)) {
            std::string name = display_get_item_label((display_item_e)i);
            writer.Key(name.c_str());
            writer.StartObject(); {
                writer.Key("value");
                writer.Double(value);
                writer.Key("source");
                writer.String(source.c_str());
                writer.Key("latency");
                writer.Int((int)(t0-time)/1000);
            }
            writer.EndObject();
        }
    writer.EndObject();

    return buffer.GetString();
}

static std::string get_ip() {
#if 0
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
        //printf("IP: " IPSTR "\n", IP2STR(&ip.ip));
        //printf("Gateway: " IPSTR "\n", IP2STR(&ip.gw));
        //        printf("Netmask: " IPSTR "\n", IP2STR(&ip.netmask));
        return IP2STR(&ip.ip);
    }
#endif
    return "<unknown>";
}

static std::string json_settings()
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    //if(var == "WIFI_NETWORKS") return wifi_networks_html;
#define S(KEY, VALUE) writer.Key(KEY); writer.String(VALUE.c_str());
#define I(KEY, VALUE) writer.Key(KEY); writer.Int(VALUE);
#define B(KEY, VALUE) writer.Key(KEY); writer.Bool(VALUE);

    S("wifi_mode", get_wifi_mode_str());
    S("ap_ssid", settings.ap_ssid);
    S("ap_psk", settings.ap_psk);
    S("client_ssid", settings.client_ssid);
    S("client_psk", settings.client_psk);
    I("wifi_channel", settings.wifi_channel);

    B("input_usb_host", settings.input_usb_host);
    B("output_usb_host", settings.output_usb_host);
    B("input_usb", settings.input_usb);
    B("output_usb", settings.output_usb);
    I("usb_baud_rate", settings.usb_baud_rate);
    I("rs422_1_baud_rate", settings.rs422_1_baud_rate);
    I("rs422_2_baud_rate", settings.rs422_2_baud_rate);

    B("input_nmea_pypilot", settings.input_nmea_pypilot);
    B("output_nmea_pypilot", settings.output_nmea_pypilot);
    B("input_nmea_signalk", settings.input_nmea_signalk);
    B("output_nmea_signalk", settings.output_nmea_signalk);
    B("input_nmea_client", settings.input_nmea_client);
    B("output_nmea_client", settings.output_nmea_client);
    B("input_nmea_server", settings.input_nmea_server);
    B("output_nmea_server", settings.output_nmea_server);
    B("input_signalk", settings.input_signalk);
    B("output_signalk", settings.output_signalk);

    B("forward_nmea_serial_to_serial", settings.forward_nmea_serial_to_serial);
    B("forward_nmea_serial_to_wifi", settings.forward_nmea_serial_to_wifi);

    B("compesate_wind_with_acclerometer", settings.compensate_wind_with_accelerometer);
    B("compute_truewind_from_gps", settings.compute_true_wind_from_gps);
    B("compute_truewind_from_waterspeed", settings.compute_true_wind_from_water);

    S("pypilot_addr",(pypilot_discovered==2 ? settings.pypilot_addr : "not detected"));

    S("signalk_addr",(signalk_discovered==2 ? (settings.signalk_addr + ":" + int_to_str(settings.signalk_port)) :( "not detected")));
    S("nmea_client_addr", settings.nmea_client_addr);
    S("nmea_client_port", int_to_str(settings.nmea_client_port));
    S("ip_address", get_ip());
    S("nmea_server_port", int_to_str(settings.nmea_server_port));
    B("use360", settings.use_360);
    B("usefarenheit", settings.use_fahrenheit);
    B("useinHg", settings.use_inHg);
    B("usedepthft", settings.use_depth_ft);
    S("lat_lon_format", settings.lat_lon_format);
    B("invert", settings.invert);
    S("contrast", int_to_str(settings.contrast));
    S("backlight", int_to_str(settings.backlight));
    B("mirror", settings.mirror);

    S("power_button", settings.power_button);
    //    if(var == "DISPLAYPAGES") return get_display_pages();

    S("version", float_to_str(VERSION, 2));

    // alarms

    B("anchor_alarm", settings.anchor_alarm);
    I("anchor_alarm_distance", settings.anchor_alarm_distance);

    B("course_alarm", settings.course_alarm);
    I("course_alarm_course", settings.course_alarm_course);
    I("course_alarm_error", settings.course_alarm_error);

    B("gps_speed_alarm", settings.gps_speed_alarm);
    I("gps_min_speed_alarm_knots", settings.gps_min_speed_alarm_knots);
    I("gps_max_speed_alarm_knots", settings.gps_max_speed_alarm_knots);

    B("wind_speed_alarm", settings.wind_speed_alarm);
    I("wind_min_speed_alarm_knots", settings.wind_min_speed_alarm_knots);
    I("wind_max_speed_alarm_knots", settings.wind_max_speed_alarm_knots);

    B("water_speed_alarm", settings.water_speed_alarm);
    I("water_min_speed_alarm_knots", settings.water_min_speed_alarm_knots);
    I("water_max_speed_alarm_knots", settings.water_max_speed_alarm_knots);

    B("weather_alarm_pressure", settings.weather_alarm_pressure);
    I("weather_alarm_min_pressure", settings.weather_alarm_min_pressure);
    B("weather_alarm_pressure_rate", settings.weather_alarm_pressure_rate);
    I("weather_alarm_pressure_rate_value", settings.weather_alarm_pressure_rate_value);
    B("weather_alarm_lightning", settings.weather_alarm_lightning);
    I("weather_alarm_lightning_distance", settings.weather_alarm_lightning_distance);

    B("depth_alarm", settings.depth_alarm);
    I("depth_alarm_min", settings.depth_alarm_min);
    B("depth_alarm_rate", settings.depth_alarm_rate);
    I("depth_alarm_rate_value", settings.depth_alarm_rate_value);

    B("ais_alarm", settings.ais_alarm);
    I("ais_alarm_cpa", settings.ais_alarm_cpa);
    I("ais_alarm_tcpa", settings.ais_alarm_tcpa);

    B("pypilot_alarm_noconnection", settings.pypilot_alarm_noconnection);
    B("pypilot_alarm_fault", settings.pypilot_alarm_fault);
    B("pypilot_alarm_no_imu", settings.pypilot_alarm_no_imu);
    B("pypilot_alarm_no_motor_controller", settings.pypilot_alarm_no_motor_controller);
    B("pypilot_alarm_lost_mode", settings.pypilot_alarm_lost_mode);

    return buffer.GetString();
}

static std::string json_sensors()
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    writer.SetMaxDecimalPlaces(2);

    sensors_write_transmitters(writer, true);
    return buffer.GetString();
}

static esp_err_t send_ws(int fd, const char *msg) {
    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)msg,
        .len = strlen(msg),
    };

    return httpd_ws_send_frame_async(server, fd, &frame);
}

static void ws_add_client(std::unordered_set<int> &clients, int fd)
{
    if(clients.size() > 10) {
        ESP_LOGI(TAG, "Too many websocket clients");
        return;
    }

    if(send_ws(fd, json_sensors().c_str()) != ESP_OK ||
       send_ws(fd, json_settings().c_str()) != ESP_OK)
        ESP_LOGW(TAG, "send to fd=%d failed", fd);
    else
        clients.insert(fd);
}

static void ws_broadcast_text(std::unordered_set<int> &clients, const char *msg)
{
    if (!msg)
        return;

    httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)msg,
        .len = strlen(msg),
    };

    for (auto i = clients.begin(); i != clients.end();) {
        int fd = *i;
        if (fd < 0)
            continue;

        esp_err_t err = httpd_ws_send_frame_async(server, fd, &frame);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "send to fd=%d failed: %s", fd, esp_err_to_name(err));
            i = clients.erase(i);
        } else
            i++;
    }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    std::unordered_set<int> *clients = reinterpret_cast<std::unordered_set<int>*>(req->user_ctx);

    int fd = httpd_req_to_sockfd(req);
    if (req->method == HTTP_GET) {
        printf("websocket client\n");
        ws_add_client(*clients, fd);
        // action before ws handshake
        return ESP_OK;
    }

    httpd_ws_frame_t frame = {
            .final = true,
            .fragmented = false,
            .type = HTTPD_WS_TYPE_TEXT,
            .payload = NULL,
            .len = 0
    };

    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ws_recv_frame len failed: %s", esp_err_to_name(err));
        clients->erase(fd);
        return err;
    }

    if (frame.len <= 0) {
        printf("websocket small frame? %d\n", frame.len);
        return ESP_OK;
    }

    if(frame.len > 256) {
        printf("websocket frame too big %d\n", frame.len);
        return ESP_OK;
    }
    
    if(clients == &ws_clients) {
        uint8_t buf[frame.len+1];
        frame.payload = buf;
        err = httpd_ws_recv_frame(req, &frame, frame.len);
        if (err != ESP_OK) {
            clients->erase(fd);
            return err;
        }

        buf[frame.len] = 0;
        ESP_LOGI(TAG, "recv from fd=%d: %s", fd, (char *)buf);

        rapidjson::Document doc;
        doc.Parse((char*)buf);

        for (rapidjson::Value::ConstMemberIterator itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr) {
            std::string name = itr->name.GetString();
            const rapidjson::Value &value = itr->value;
            std::string v = value.GetString();
            if(name == "wifi_mode") {
                if (v == "ap") settings.wifi_mode = WIFI_MODE_AP;
                if (v == "client") settings.wifi_mode = WIFI_MODE_STA;
                if (v == "none") settings.wifi_mode = WIFI_MODE_NULL;
                else printf("unknown wifi data setting %s\n", v.c_str());
            }
            else if(name == "ap_ssid")    settings.ap_ssid = v;
            else if(name == "ap_psk")     settings.ap_psk = v;
            else if(name == "client_ssid") settings.client_ssid = v;
            else if(name == "client_psk") settings.client_psk = v;
            else if(name == "wifi_channel") settings.wifi_channel = str_to_int(v);

            else if(name == "use_360")        settings.use_360 = value.GetBool();
            else if(name == "use_fahrenheit") settings.use_fahrenheit = value.GetBool();
            else if(name == "use_inHg")       settings.use_inHg = value.GetBool();
            else if(name == "use_depthft")    settings.use_depth_ft = value.GetBool();
            else if(name == "lat_lon_format") settings.lat_lon_format = v;
            else if(name == "invert")         settings.invert = value.GetBool();
            else if(name == "contrast")       settings.contrast = std::min(std::max(str_to_int(v), 0), 50);
            else if(name == "backlight")      settings.backlight = std::min(std::max(str_to_int(v), 0), 20);
            else if(name == "mirror")         settings.mirror = str_to_int(v);
            else if(name == "power_button")   settings.power_button = v;

            // alarms
            else if(name == "anchor_alarm")           settings.anchor_alarm = value.GetBool();
            else if(name == "anchor_alarm_distance")  settings.anchor_alarm_distance = value.GetInt();

            else if(name == "course_alarm")         settings.course_alarm = value.GetBool();
            else if(name == "course_alarm_course")  settings.course_alarm_course = value.GetInt();
            else if(name == "course_alarm_error")   settings.course_alarm_error = value.GetInt();
            
            else if(name == "gps_speed_alarm")        settings.gps_speed_alarm = value.GetBool();
            else if(name == "gps_min_speed_alarm_knots")  settings.gps_min_speed_alarm_knots = value.GetInt();
            else if(name == "gps_max_speed_alarm_knots")  settings.gps_max_speed_alarm_knots = value.GetInt();

            else if(name == "wind_speed_alarm")        settings.wind_speed_alarm = value.GetBool();
            else if(name == "wind_min_speed_alarm_knots")  settings.wind_min_speed_alarm_knots = value.GetInt();
            else if(name == "wind_max_speed_alarm_knots")  settings.wind_max_speed_alarm_knots = value.GetInt();

            else if(name == "water_speed_alarm")        settings.water_speed_alarm = value.GetBool();
            else if(name == "water_min_speed_alarm_knots")  settings.water_min_speed_alarm_knots = value.GetInt();
            else if(name == "water_max_speed_alarm_knots")  settings.water_max_speed_alarm_knots = value.GetInt();

            else if(name == "weather_alarm_pressure")      settings.weather_alarm_pressure = value.GetBool();
            else if(name == "weather_alarm_min_pressure")  settings.weather_alarm_min_pressure = value.GetInt();
            else if(name == "weather_alarm_pressure_rate")      settings.weather_alarm_pressure_rate = value.GetBool();
            else if(name == "weather_alarm_pressure_rate_value")  settings.weather_alarm_pressure_rate_value = value.GetInt();
            else if(name == "weather_alarm_lightning")      settings.weather_alarm_lightning = value.GetBool();
            else if(name == "weather_alarm_lightning_distance")  settings.weather_alarm_lightning_distance = value.GetInt();

            else if(name == "depth_alarm")      settings.depth_alarm = value.GetBool();
            else if(name == "depth_alarm_min")  settings.depth_alarm_min = value.GetInt();
            else if(name == "depth_alarm_rate")      settings.depth_alarm_rate = value.GetBool();
            else if(name == "depth_alarm_rate_value")  settings.depth_alarm_rate_value = value.GetInt();

            else if(name == "ais_alarm")      settings.ais_alarm = value.GetBool();
            else if(name == "ais_alarm_cpa")  settings.ais_alarm_cpa = value.GetInt();
            else if(name == "ais_alarm_tcpa")      settings.ais_alarm_tcpa = value.GetInt();
            
            else if(name == "pypilot_alarm_noconnection") settings.pypilot_alarm_noconnection = value.GetBool();
            else if(name == "pypilot_alarm_fault") settings.pypilot_alarm_fault = value.GetBool();
            else if(name == "pypilot_alarm_no_imu") settings.pypilot_alarm_no_imu = value.GetBool();
            else if(name == "pypilot_alarm_no_motor_controller") settings.pypilot_alarm_no_motor_controller = value.GetBool();
            else if(name == "pypilot_alarm_lost_mode") settings.pypilot_alarm_lost_mode = value.GetBool();

            // commands
            else if(name == "scan_wifi")        wireless_scan_networks();
#ifdef CONFIG_IDF_TARGET_ESP32S3
            else if(name == "display_auto")     display_auto();
#endif
            else if(name == "program_channel")  void wireless_program_channel();

            // display pages
#ifdef CONFIG_IDF_TARGET_ESP32S3
            else if(name.rfind("page", 0) == 0) {
                for(int j=0; j < display_pages.size(); j++) {
                    page_info &page = display_pages[j];
                    if(page.name == name[4])
                        page.enabled = value.GetBool();
            }
#endif            
            else printf("web post unknown parameter %s %s\n", name.c_str(), v.c_str());
        }

        // with keys of mac address of sensors, update wireless settings
        sensors_wireless_setting(doc);
    }
    return ESP_OK;
}

static bool endswith(const char *name, const char *str) {
    int len = strlen(name);
    int slen = strlen(str);
    if(slen > len)
        return false;
    return !strcmp(name+len-slen, str);
}

static esp_err_t display_pages_handler(httpd_req_t *req)
{
#ifdef CONFIG_IDF_TARGET_ESP32S3
    std::string pages = "";
    for(std::vector<page_info>::iterator it = display_pages.begin(); it!=display_pages.end(); it++) {
        std::string cn(1, it->name);
        pages += "<span><input type='checkbox' name='" + cn + "' id='page" + cn + "'";
        if(it->enabled)
            pages += " checked";
        pages += "><label for='page" + cn + "'><b>";
        pages += it->name;
        pages += "</b> " + it->description + "</label></span>\n";
    }
    httpd_resp_sendstr(req, pages.c_str());
#else
    httpd_resp_sendstr(req, "");
#endif
    return ESP_OK;
}

static esp_err_t update_handler(httpd_req_t *req)
{
    char buf[4096];
    int remaining = req->content_len;
    int received;
    esp_err_t err;

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "No OTA partition available");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle;
    // TODO: set size to content_len?  saves erasing as much flash
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, esp_err_to_name(err));
        return err;
    }

    while (remaining > 0) {
        received = httpd_req_recv(req, buf, std::min(remaining, (int)sizeof(buf)));
        if (received == HTTPD_SOCK_ERR_TIMEOUT)
            continue;

        if (received <= 0) {
            esp_ota_abort(ota_handle);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_sendstr(req, "Receive failed");
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, received);
        if (err != ESP_OK) {
            esp_ota_abort(ota_handle);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_sendstr(req, esp_err_to_name(err));
            return err;
        }

        remaining -= received;
            
        vTaskDelay(pdMS_TO_TICKS(1)); // this is needed to avoid watchdog resets
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, esp_err_to_name(err));
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, esp_err_to_name(err));
        return err;
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, "<html><body><h1>Update success</h1><p>Rebooting...</p></body></html>");

    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    char path[128];
    strncpy(path, req->uri, sizeof(path));
    path[sizeof path - 1] = 0;

    char *q = strchr(path, '?');
    if (q)
        *q = 0;

    if(!strcmp(path, "/"))
        strcpy(path, "/index.html");
    
    unsigned int i, num_files = (sizeof data_files)/(sizeof *data_files);
    for(i = 0; i<num_files; i++) {
        if(!strcmp(path+1, data_files[i].path))
            break;
    }
        
    if(i == num_files) {
        httpd_resp_sendstr(req, "file not found");
        printf("did not find %s\n", path);
        return ESP_OK;
    }
    
    int size = data_files[i].size;
    const char *data = data_files[i].data;

    const int max_len = 1024;
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    if(endswith(path, ".html"))
        httpd_resp_set_type(req, "text/html");
    else if(endswith(path, ".js"))
        httpd_resp_set_type(req, "text/javascript");
    else if(endswith(path, ".css"))
        httpd_resp_set_type(req, "text/css");
    else
        httpd_resp_set_type(req, "text/plain");

    for (int off = 0; off < size; ) {
        int chunk = size - off;
        if (chunk > max_len) chunk = max_len;
        httpd_resp_send_chunk(req, data + off, chunk);
        off += chunk;
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    
    return ESP_OK;
}

void web_setup() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
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
        .user_ctx  = &ws_clients,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server, &ws);

    static const httpd_uri_t ws_data = {
        .uri       = "/ws_data",
        .method    = (httpd_method_t)HTTP_GET,
        .handler   = ws_handler,
        .user_ctx  = &ws_data_clients,
        .is_websocket = true,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server, &ws_data);

    static const httpd_uri_t display_pages = {
        .uri       = "/display_pages",
        .method    = (httpd_method_t)HTTP_GET,
        .handler   = display_pages_handler,
        .user_ctx  = NULL,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server, &display_pages);

    static const httpd_uri_t update = {
        .uri       = "/update",
        .method    = (httpd_method_t)HTTP_POST,
        .handler   = update_handler,
        .user_ctx  = NULL,
        .is_websocket = false,
        .handle_ws_control_frames = false,
        .supported_subprotocol = NULL
    };
    httpd_register_uri_handler(server, &update);

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

void web_poll() {
    uint64_t t = esp_timer_get_time();

    static uint64_t last_sock_update;
    if(t - last_sock_update < 200e3)
        return;
    last_sock_update = t;

    if(ws_clients.size())
    {
        std::string s = jsonCurrent();
        if(!s.empty()) {
            //println("ws: " + s);
            ws_broadcast_text(ws_clients, s.c_str());
        }
    }

    static uint64_t last_client_update;
    if(t - last_client_update < 2e6)
        return;

    last_client_update = t;

    if(ws_clients.size())
        ws_broadcast_text(ws_clients, json_sensors().c_str());

    if(ws_data_clients.size())
        ws_broadcast_text(ws_data_clients, jsonDisplayData().c_str());
}
