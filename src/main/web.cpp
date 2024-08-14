/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <iostream>

#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "settings.h"
#include "wireless.h"
#include "display.h"
#include "utils.h"
#include "history.h"
#include "zeroconf.h"
#include "signalk.h"
#include "web.h"

#include "data.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocket ws_data("/ws_data");

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        printf("websocket got %s\n", data);
        rapidjson::Document input;
        input.Parse((char*)data);

        wireless_setting(input);
    }
}

static std::string jsonDisplayData() {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    writer.SetMaxDecimalPlaces(2);
    float value;
    std::string source;
    uint32_t time, t0=millis();
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
                writer.Int((int)(t0-time));
            }
            writer.EndObject();
        }
    writer.EndObject();

    return buffer.GetString();
}

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

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type) {
        case WS_EVT_CONNECT:
            printf("WebSocket client #%lu connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            ws.textAll(wireless_json_sensors().c_str());
            break;
        case WS_EVT_DISCONNECT:
            printf("WebSocket client #%lu disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

std::string get_display_pages()
{
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
    return pages;
}

std::string Checked(bool value) {
    return value ? "checked" : "";
}

std::string processor(const std::string& var)
{
    //println(var);
    if(var == "WIFI_NETWORKS") return wifi_networks_html;
    if(var == "SSID")       return settings.ssid;
    if(var == "PSK")        return settings.psk;
    if(var == "CHANNEL")    return int_to_str(settings.channel);
    if(var == "INPUTUSB")   return Checked(settings.input_usb);
    if(var == "OUTPUTUSB")  return Checked(settings.output_usb);
    if(var == "INPUTWIFI")  return Checked(settings.input_wifi);
    if(var == "OUTPUTWIFI") return Checked(settings.output_wifi);
    if(var == "USB_BAUD_RATE") return int_to_str(settings.usb_baud_rate);
    if(var == "RS422_BAUD_RATE") return int_to_str(settings.rs422_baud_rate);
    if(var == "COMPENSATE_WIND_WITH_ACCELEROMETER") return Checked(settings.compensate_wind_with_accelerometer);
    if(var == "COMPUTE_TRUEWIND_FROM_GPS") return Checked(settings.compute_true_wind_from_gps);
    if(var == "COMPUTE_TRUEWIND_FROM_WATERSPEED") return Checked(settings.compute_true_wind_from_water);
    if(var == "PYPILOT_ADDR") return(pypilot_discovered==2 ? settings.pypilot_addr : "not detected");
    if(var == "SIGNALK_ADDR") return(signalk_discovered==2 ? (settings.signalk_addr + ":" + int_to_str(settings.signalk_port)) : "not detected");
    if(var == "WIFIDATA")   return get_wifi_data_str();
    if(var == "NMEACLIENTADDR") return settings.nmea_client_addr;
    if(var == "NMEACLIENTPORT") return int_to_str(settings.nmea_client_port);
    if(var == "IPADDRESS") return WiFi.localIP().toString().c_str();    
    if(var == "NMEASERVERPORT") return int_to_str(settings.nmea_server_port);
    if(var == "USE360")  return Checked(settings.use_360);
    if(var == "USEFAHRENHEIT")  return Checked(settings.use_fahrenheit);
    if(var == "USEINHG")  return Checked(settings.use_inHg);
    if(var == "USEDEPTHFT")  return Checked(settings.use_depth_ft);
    if(var == "LATLONFORMAT")  return settings.lat_lon_format;
    if(var == "INVERT")  return Checked(settings.invert);
    if(var == "CONTRAST")  return int_to_str(settings.contrast);
    if(var == "BACKLIGHT")  return int_to_str(settings.backlight);
    if(var == "MIRROR")  return Checked(settings.mirror);
    if(var == "POWERBUTTON")  return settings.power_button;
    if(var == "DISPLAYPAGES") return get_display_pages();
    if(var == "VERSION")    return float_to_str(VERSION, 2);
    return std::string();
}

String processor_helper(const String &c)
{
    return processor(c.c_str()).c_str();
}

std::string processor_alarms(const std::string& var)
{
    if(var == "ANCHOR_ALARM")          return Checked(settings.anchor_alarm);
    if(var == "ANCHOR_ALARM_DISTANCE") return int_to_str(settings.anchor_alarm_distance);

    if(var == "COURSE_ALARM")          return Checked(settings.course_alarm);
    if(var == "COURSE_ALARM_COURSE")   return int_to_str(settings.course_alarm_course);
    if(var == "COURSE_ALARM_ERROR")    return int_to_str(settings.course_alarm_error);

    if(var == "GPS_SPEED_ALARM")          return Checked(settings.gps_speed_alarm);
    if(var == "GPS_MIN_SPEED_ALARM_KNOTS")    return int_to_str(settings.gps_min_speed_alarm_knots);
    if(var == "GPS_MAX_SPEED_ALARM_KNOTS")    return int_to_str(settings.gps_max_speed_alarm_knots);

    if(var == "WIND_SPEED_ALARM")          return Checked(settings.wind_speed_alarm);
    if(var == "WIND_MIN_SPEED_ALARM_KNOTS")    return int_to_str(settings.wind_min_speed_alarm_knots);
    if(var == "WIND_MAX_SPEED_ALARM_KNOTS")    return int_to_str(settings.wind_max_speed_alarm_knots);

    if(var == "WATER_SPEED_ALARM")          return Checked(settings.water_speed_alarm);
    if(var == "WATER_MIN_SPEED_ALARM_KNOTS")    return int_to_str(settings.water_min_speed_alarm_knots);
    if(var == "WATER_MAX_SPEED_ALARM_KNOTS")    return int_to_str(settings.water_max_speed_alarm_knots);

    if(var == "WEATHER_ALARM_PRESSURE")      return Checked(settings.weather_alarm_pressure);
    if(var == "WEATHER_ALARM_MIN_PRESSURE")  return int_to_str(settings.weather_alarm_min_pressure);
    if(var == "WEATHER_ALARM_PRESSURE_RATE")       return Checked(settings.weather_alarm_pressure_rate);
    if(var == "WEATHER_ALARM_PRESSURE_RATE_VAL") return int_to_str(settings.weather_alarm_pressure_rate_value);
    if(var == "WEATHER_ALARM_LIGHTNING")          return Checked(settings.weather_alarm_lightning);
    if(var == "WEATHER_ALARM_LIGHTNING_DISTANCE") return int_to_str(settings.weather_alarm_lightning_distance);

    if(var == "DEPTH_ALARM")             return Checked(settings.depth_alarm);
    if(var == "DEPTH_ALARM_MIN")         return int_to_str(settings.depth_alarm_min);
    if(var == "DEPTH_ALARM_RATE")        return Checked(settings.depth_alarm_rate);
    if(var == "DEPTH_ALARM_RATE_VALUE")  return int_to_str(settings.depth_alarm_rate_value);

    if(var == "AIS_ALARM")       return Checked(settings.ais_alarm);
    if(var == "AIS_ALARM_CPA")   return int_to_str(settings.ais_alarm_cpa);
    if(var == "AIS_ALARM_TCPA")  return int_to_str(settings.ais_alarm_tcpa);

    if(var == "PYPILOT_ALARM_NOCONNECTION")       return Checked(settings.pypilot_alarm_noconnection);
    if(var == "PYPILOT_ALARM_FAULT")       return Checked(settings.pypilot_alarm_fault);
    if(var == "PYPILOT_ALARM_NO_IMU")       return Checked(settings.pypilot_alarm_no_imu);
    if(var == "PYPILOT_ALARM_NO_MOTORCONTROLLER")       return Checked(settings.pypilot_alarm_no_motor_controller);
    if(var == "PYPILOT_ALARM_LOST_MODE")       return Checked(settings.pypilot_alarm_lost_mode);
    
    if(var == "VERSION")    return float_to_str(VERSION, 2);
    return std::string();
}

String processor_alarms_helper(const String &c)
{
    return processor_alarms(c.c_str()).c_str();
}


static ArRequestHandlerFunction getRequest(const char *path, AwsTemplateProcessor processor=nullptr)
{
    return [path, processor](AsyncWebServerRequest *request) {
        unsigned int i, num_files = (sizeof data_files)/(sizeof *data_files);
        for(i = 0; i<num_files; i++)
            if(!strcmp(path, data_files[i].path))
                break;
        if(i == num_files) {
            printf("file not found");
            return;
        }
        
        int size = data_files[i].size;
        const char *data = data_files[i].data;
        String p(path);

        String type = "text/plain";
        if(p.endsWith(".html"))
           type = "text/html";
        else if(p.endsWith(".js"))
           type = "text/javascript";
        else if(p.endsWith(".css"))
           type = "text/css";
            
        request->send(type, size, [size, data](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            int r = size - index;
            int len = r > maxLen ? maxLen : r;
            memcpy(buffer, data + index, len);
            return len;
        }, processor);
    };
}

void web_setup()
{
    ws.onEvent(onEvent);
    ws_data.onEvent(onEvent);
    server.addHandler(&ws);
    server.addHandler(&ws_data);

    server.on("/", HTTP_GET, getRequest("index.html", processor_helper));

    server.on("/alarms", HTTP_POST, [](AsyncWebServerRequest *request) {
        settings.anchor_alarm = false;
        settings.course_alarm = false;
        settings.gps_speed_alarm = false;
        settings.wind_speed_alarm = false;
        settings.water_speed_alarm = false;
        settings.weather_alarm_pressure = false;
        settings.weather_alarm_pressure_rate = false;
        settings.weather_alarm_lightning = false;
        settings.depth_alarm = false;
        settings.depth_alarm_rate = false;
        settings.ais_alarm = false;
        settings.pypilot_alarm_noconnection = false;
        settings.pypilot_alarm_fault = false;
        settings.pypilot_alarm_no_imu = false;
        settings.pypilot_alarm_no_motor_controller = false;
        settings.pypilot_alarm_lost_mode = false;

        for (int i = 0; i < request->params(); i++) {
            AsyncWebParameter* p = request->getParam(i);
            String x = p->name();
            std::string name = x.c_str();
            x = p->value();
            std::string value = x.c_str();
            //printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            if(name == "anchor_alarm")           settings.anchor_alarm = true;
            else if(name == "anchor_alarm_distance")  settings.anchor_alarm_distance = str_to_int(value);

            else if(name == "course_alarm")         settings.course_alarm = true;
            else if(name == "course_alarm_course")  settings.course_alarm_course = str_to_int(value);
            else if(name == "course_alarm_error")   settings.course_alarm_error = str_to_int(value);
            
            else if(name == "gps_speed_alarm")        settings.gps_speed_alarm = true;
            else if(name == "gps_min_speed_alarm_knots")  settings.gps_min_speed_alarm_knots = str_to_int(value);
            else if(name == "gps_max_speed_alarm_knots")  settings.gps_max_speed_alarm_knots = str_to_int(value);

            else if(name == "wind_speed_alarm")        settings.wind_speed_alarm = true;
            else if(name == "wind_min_speed_alarm_knots")  settings.wind_min_speed_alarm_knots = str_to_int(value);
            else if(name == "wind_max_speed_alarm_knots")  settings.wind_max_speed_alarm_knots = str_to_int(value);

            else if(name == "water_speed_alarm")        settings.water_speed_alarm = true;
            else if(name == "water_min_speed_alarm_knots")  settings.water_min_speed_alarm_knots = str_to_int(value);
            else if(name == "water_max_speed_alarm_knots")  settings.water_max_speed_alarm_knots = str_to_int(value);

            else if(name == "weather_alarm_pressure")      settings.weather_alarm_pressure = true;
            else if(name == "weather_alarm_min_pressure")  settings.weather_alarm_min_pressure = str_to_int(value);
            else if(name == "weather_alarm_pressure_rate")      settings.weather_alarm_pressure_rate = true;
            else if(name == "weather_alarm_pressure_rate_value")  settings.weather_alarm_pressure_rate_value = str_to_int(value);
            else if(name == "weather_alarm_lightning")      settings.weather_alarm_lightning = true;
            else if(name == "weather_alarm_lightning_distance")  settings.weather_alarm_lightning_distance = str_to_int(value);

            else if(name == "depth_alarm")      settings.depth_alarm = true;
            else if(name == "depth_alarm_min")  settings.depth_alarm_min = str_to_int(value);
            else if(name == "depth_alarm_rate")      settings.depth_alarm_rate = true;
            else if(name == "depth_alarm_rate_value")  settings.depth_alarm_rate_value = str_to_int(value);

            else if(name == "ais_alarm")      settings.ais_alarm = true;
            else if(name == "ais_alarm_cpa")  settings.ais_alarm_cpa = str_to_int(value);
            else if(name == "ais_alarm_tcpa")      settings.ais_alarm_tcpa = str_to_int(value);
            
            else if(name == "pypilot_alarm_noconnection") settings.pypilot_alarm_noconnection = true;
            else if(name == "pypilot_alarm_fault") settings.pypilot_alarm_fault = true;
            else if(name == "pypilot_alarm_no_imu") settings.pypilot_alarm_no_imu = true;
            else if(name == "pypilot_alarm_no_motor_controller") settings.pypilot_alarm_no_motor_controller = true;
            else if(name == "pypilot_alarm_lost_mode") settings.pypilot_alarm_lost_mode = true;
            
            else printf("web post alarms unknown parameter %s %s\n", name.c_str(), value.c_str());
        }
        request->redirect("/alarms.html");
        settings_store();
    });

//    server.on("/alarms.html", HTTP_GET, [](AsyncWebServerRequest *request) {
//        request->send(SPIFFS, "/alarms.html", String(), 0, processor_alarms_helper);
    server.on("/alarms.html", HTTP_GET, getRequest("alarms.html", processor_alarms_helper));

    server.on("/network", HTTP_POST, [](AsyncWebServerRequest *request) {
        //printf("post network\n");
        for (int i = 0; i < request->params(); i++) {
            AsyncWebParameter* p = request->getParam(i);
            std::string name = p->name().c_str(), value = p->value().c_str();
            //printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            if(name == "ssid")     settings.ssid = value;
            else if(name == "psk") settings.psk = value;
            else printf("web post wifi unknown parameter %s %s\n", name.c_str(), value.c_str());
        }
        request->redirect("/");
        settings_store();
    });

    server.on("/scan_wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
        wireless_scan_networks();
        request->redirect("/");
    });

    server.on("/scan_sensors", HTTP_POST, [](AsyncWebServerRequest *request) {
        wireless_scan_devices();
        request->redirect("/");
    });

    server.on("/data", HTTP_POST, [](AsyncWebServerRequest *request) {
        //printf("post data %d\n", request->params());
        settings.input_usb = settings.output_usb = false;
        settings.input_wifi = settings.output_wifi = false;
        settings.compensate_wind_with_accelerometer = false;
        settings.compute_true_wind_from_gps = false;
        settings.compute_true_wind_from_water = false;
        for (int i = 0; i < request->params(); i++) {
            AsyncWebParameter* p = request->getParam(i);
            std::string name = p->name().c_str(), value = p->value().c_str();
            //printf("POST[%s]: %s\n", name.c_str(), value.c_str());
            if     (name == "input_usb")   settings.input_usb = true;
            else if(name == "output_usb")  settings.output_usb = true;
            else if(name == "usb_baud_rate")  settings.usb_baud_rate = str_to_int(value);
            else if(name == "rs422_baud_rate")  settings.rs422_baud_rate = str_to_int(value);
            else if(name == "input_wifi")  settings.input_wifi = true;
            else if(name == "output_wifi") settings.output_wifi = true;
            else if(name == "compensate_wind_with_accelerometer") settings.compensate_wind_with_accelerometer = true;
            else if(name == "compute_true_wind_from_gps") settings.compute_true_wind_from_gps = true;
            else if(name == "compute_true_wind_from_water_speed") settings.compute_true_wind_from_water = true;
            else if(name == "wifidata") {
                if      (value == "nmea_pypilot") settings.wifi_data = NMEA_PYPILOT;
                else if (value == "nmea_signalk") settings.wifi_data = NMEA_SIGNALK;
                else if (value == "nmea_client") settings.wifi_data = NMEA_CLIENT;
                else if (value == "nmea_server") settings.wifi_data =  NMEA_SERVER;
                else if (value == "signalk") settings.wifi_data =  SIGNALK;
                else printf("post unknown wifi data setting %s\n", value.c_str());
            }
            else if(name == "nmea_tcp_client_addr") settings.nmea_client_addr = value;
            else if(name == "nmea_tcp_client_port") settings.nmea_client_port = str_to_int(value);
            else if(name == "nmea_tcp_server_port") settings.nmea_server_port = str_to_int(value);
            else printf("web post data unknown parameter %s %s\n", name.c_str(), value.c_str());
        }
        request->redirect("/");
        settings_store();
    });

    server.on("/display", HTTP_POST, [](AsyncWebServerRequest *request) {
        settings.use_360 = settings.use_fahrenheit = false;
        settings.use_inHg = settings.use_depth_ft = false;
        settings.invert = false;
        settings.mirror = false;
        std::string enabled_pages = "";
        for(int i=0; i < display_pages.size(); i++)
            display_pages[i].enabled = false;
        for (int i = 0; i < request->params(); i++) {
            AsyncWebParameter* p = request->getParam(i);
            std::string name = p->name().c_str(), value = p->value().c_str();
            //printf("POST[%s]: %s\n", name.c_str(), value.c_str());
            if(name == "use360")
                settings.use_360 = true;
            else if(name == "usefahrenheit")
                settings.use_fahrenheit = true;
            else if(name == "useinHg")
                settings.use_inHg = true;
            else if(p->name() == "usedepthft")
                settings.use_depth_ft = true;
            else if(p->name() == "lat_lon_format")
                settings.lat_lon_format = value;
            else if(name == "invert")
                settings.invert = true;
            else if(p->name() == "contrast")
                settings.contrast = min(max(str_to_int(value), 0), 50);
            else if(p->name() == "backlight")
                settings.backlight = min(max(str_to_int(value), 0), 20);
            else if(p->name() == "mirror")
                settings.mirror = true;
            else if(p->name() == "power_button")
                settings.power_button = value;
            else {
                for(int j=0; j < display_pages.size(); j++) {
                    page_info &page = display_pages[j];
                    if(std::string(1, page.name) == name)
                        page.enabled = true;
                }
                enabled_pages += name;
            }
        }
        settings.enabled_pages = enabled_pages;
        request->redirect("/");
        settings_store();
        signalk_subscribe();
        //display_set_mirror_rotation();
        display_change_page(0);
    });

    server.on("/display_auto", HTTP_POST, [](AsyncWebServerRequest *request) {
        display_auto();
        request->redirect("/");
    });

    server.on("/history", HTTP_GET, [](AsyncWebServerRequest *request) {
        std::string data_type, data_range;
        if (request->hasParam("data_type"))
            data_type = request->getParam("data_type")->value().c_str();
        if (request->hasParam("data_range"))
            data_range = request->getParam("data_range")->value().c_str();

        //printf("GET HISTORY %s %s\n", data_type.c_str(), data_range.c_str());

        int item = -1, range = -1;
        for(int i=0; i<DISPLAY_COUNT; i++)
            if(display_get_item_label((display_item_e)i) == data_type) {
                item = i;
                break;
            }

        for(int i=0; i<HISTORY_RANGE_COUNT; i++)
            if(history_get_label((history_range_e)i) == data_range) {
                range = i;
                break;
            }

        //printf("GET HISTORY %d %d\n", item, range);

        std::string data = history_get_data((display_item_e)item,(history_range_e)range);
        //printf("GET HISTORY %s\n", data.c_str());

        request->send(200, "text/plain", data.c_str());
    });

    // serve all files
    unsigned int i, num_files = (sizeof data_files)/(sizeof *data_files);
    for(i = 0; i<num_files; i++) {
        const char *path = data_files[i].path;
        std::string spath("/");
        spath += path;
        server.on(spath.c_str(), HTTP_GET, getRequest(path));
    }

    server.begin();
    printf("web server listening\n");
}

void web_poll()
{
    ws.cleanupClients();

    uint32_t t = millis();

    static uint32_t last_sock_update;
    if(t - last_sock_update < 200)
        return;
    last_sock_update = t;

    if(ws.count())
    {
        std::string s = jsonCurrent();
        if(!s.empty()) {
            //println("ws: " + s);
            ws.textAll(s.c_str());
        }
    }

    static uint32_t last_client_update;
    if(t - last_client_update < 2000)
        return;
    last_client_update = t;

    if(ws.count())
        ws.textAll(wireless_json_sensors().c_str());
    ws.cleanupClients();

    if(ws_data.count())
        ws_data.textAll(jsonDisplayData().c_str());
    ws_data.cleanupClients();
}
