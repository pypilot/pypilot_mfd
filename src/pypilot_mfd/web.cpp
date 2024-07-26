/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include <Arduino_JSON.h>

#include "settings.h"
#include "wireless.h"
#include "display.h"
#include "utils.h"
#include "history.h"
#include "zeroconf.h"
#include "signalk.h"
#include "web.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncWebSocket ws_data("/ws_data");

// bad put put here??
JSONVar history_get_data(display_item_e item, history_range_e range);

static sensor_position str2position(String p) {
    if(p == "Primary")   return PRIMARY;
    if(p == "Secondary") return SECONDARY;
    if(p == "Port")      return PORT;
    if(p == "Starboard") return STARBOARD;
    return IGNORED;
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        String message = (char*)data;
        Serial.printf("websocket got %s\n", data);
        JSONVar input = JSON.parse(message);

#if 0
        for(std::map<uint64_t, wind_transmitter_t>::iterator i = wind_transmitters.begin(); i != wind_transmitters.end(); i++) { 
            String mac = mac_int_to_str(i->first);
            wind_transmitter_t &t = i->second;
            if(input.hasOwnProperty(mac)) {
                JSONVar s = input[mac];
                if(s.hasOwnProperty("offset")) {
                    float offset = (double)s["offset"];
                    offset=fminf(fmaxf(offset, -180), 180);
                    t.offset = offset;
                }
                if(s.hasOwnProperty("position"))
                    t.position = str2position(s["position"]);
                break;
            }
        }
#endif
    }
}

static String jsonDisplayData() {
    JSONVar displaydata;
    float value;
    String source;
    uint32_t time, t0=millis();
    for(int i=0; i<DISPLAY_COUNT; i++)
        if(display_data_get((display_item_e)i, value, source, time)) {
            JSONVar ji;
            ji["value"] = value;
            ji["source"] = source;
            ji["latency"] = t0-time;
            String name = display_get_item_label((display_item_e)i);
            displaydata[name] = ji;
        }
    return JSON.stringify(displaydata);
}

static String jsonCurrent()
{
    JSONVar j;
    j["direction"] = lpwind_dir;
    j["knots"] = wind_knots;

    JSONVar l;
    l["wind"] = j;
    return JSON.stringify(l);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            ws.textAll(wireless_json_sensors());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

String get_display_pages()
{
    String pages = "";
    for(std::vector<page_info>::iterator it = display_pages.begin(); it!=display_pages.end(); it++) {
        pages += "<span><input type='checkbox' name='" + String(it->name) + "' id='page" + String(it->name) + "'";
        if(it->enabled)
            pages += " checked";
        pages += "><label for='page" + String(it->name) + "'><b>";
        pages += it->name;
        pages += "</b> " + it->description + "</label></span>\n";
    }
    return pages;
}

String Checked(bool value) {
    return value ? "checked" : "";
}

String processor(const String& var)
{
    //Serial.println(var);
    if(var == "WIFI_NETWORKS") return wifi_networks_html;
    if(var == "SSID")       return settings.ssid;
    if(var == "PSK")        return settings.psk;
    if(var == "CHANNEL")    return String(settings.channel);
    if(var == "INPUTUSB")   return Checked(settings.input_usb);
    if(var == "OUTPUTUSB")  return Checked(settings.output_usb);
    if(var == "INPUTWIFI")  return Checked(settings.input_wifi);
    if(var == "OUTPUTWIFI") return Checked(settings.output_wifi);
    if(var == "USB_BAUD_RATE") return String(settings.usb_baud_rate);
    if(var == "RS422_BAUD_RATE") return String(settings.rs422_baud_rate);
    if(var == "COMPENSATE_WIND_WITH_ACCELEROMETER") return Checked(settings.compensate_wind_with_accelerometer);
    if(var == "COMPUTE_TRUEWIND_FROM_GPS") return Checked(settings.compute_true_wind_from_gps);
    if(var == "COMPUTE_TRUEWIND_FROM_WATERSPEED") return Checked(settings.compute_true_wind_from_water);
    if(var == "PYPILOT_ADDR") return(pypilot_discovered==2 ? settings.pypilot_addr : "not detected");
    if(var == "SIGNALK_ADDR") return(signalk_discovered==2 ? (settings.signalk_addr + ":" + String(settings.signalk_port)) : "not detected");
    if(var == "WIFIDATA")   return get_wifi_data_str();
    if(var == "NMEACLIENTADDR") return String(settings.nmea_client_addr);
    if(var == "NMEACLIENTPORT") return String(settings.nmea_client_port);
    if(var == "IPADDRESS") return WiFi.localIP().toString();    
    if(var == "NMEASERVERPORT") return String(settings.nmea_server_port);
    if(var == "USE360")  return Checked(settings.use_360);
    if(var == "USEFAHRENHEIT")  return Checked(settings.use_fahrenheit);
    if(var == "USEINHG")  return Checked(settings.use_inHg);
    if(var == "USEDEPTHFT")  return Checked(settings.use_depth_ft);
    if(var == "LATLONFORMAT")  return settings.lat_lon_format;
    if(var == "INVERT")  return Checked(settings.invert);
    if(var == "CONTRAST")  return String(settings.contrast);
    if(var == "BACKLIGHT")  return String(settings.backlight);
    if(var == "MIRROR")  return Checked(settings.mirror);
    if(var == "POWERDOWN")  return Checked(settings.powerdown);
    if(var == "DISPLAYPAGES") return get_display_pages();
    if(var == "VERSION")    return String(VERSION);
    return String();
}

String processor_alarms(const String& var)
{
    if(var == "ANCHOR_ALARM")          return Checked(settings.anchor_alarm);
    if(var == "ANCHOR_ALARM_DISTANCE") return String(settings.anchor_alarm_distance);

    if(var == "COURSE_ALARM")          return Checked(settings.course_alarm);
    if(var == "COURSE_ALARM_COURSE")   return String(settings.course_alarm_course);
    if(var == "COURSE_ALARM_ERROR")    return String(settings.course_alarm_error);

    if(var == "GPS_SPEED_ALARM")          return Checked(settings.gps_speed_alarm);
    if(var == "GPS_MIN_SPEED_ALARM_KNOTS")    return String(settings.gps_min_speed_alarm_knots);
    if(var == "GPS_MAX_SPEED_ALARM_KNOTS")    return String(settings.gps_max_speed_alarm_knots);

    if(var == "WIND_SPEED_ALARM")          return Checked(settings.wind_speed_alarm);
    if(var == "WIND_MIN_SPEED_ALARM_KNOTS")    return String(settings.wind_min_speed_alarm_knots);
    if(var == "WIND_MAX_SPEED_ALARM_KNOTS")    return String(settings.wind_max_speed_alarm_knots);

    if(var == "WATER_SPEED_ALARM")          return Checked(settings.water_speed_alarm);
    if(var == "WATER_MIN_SPEED_ALARM_KNOTS")    return String(settings.water_min_speed_alarm_knots);
    if(var == "WATER_MAX_SPEED_ALARM_KNOTS")    return String(settings.water_max_speed_alarm_knots);

    if(var == "WEATHER_ALARM_PRESSURE")      return Checked(settings.weather_alarm_pressure);
    if(var == "WEATHER_ALARM_MIN_PRESSURE")  return String(settings.weather_alarm_min_pressure);
    if(var == "WEATHER_ALARM_PRESSURE_RATE")       return Checked(settings.weather_alarm_pressure_rate);
    if(var == "WEATHER_ALARM_PRESSURE_RATE_VAL") return String(settings.weather_alarm_pressure_rate_value);
    if(var == "WEATHER_ALARM_LIGHTNING")          return Checked(settings.weather_alarm_lightning);
    if(var == "WEATHER_ALARM_LIGHTNING_DISTANCE") return String(settings.weather_alarm_lightning_distance);

    if(var == "DEPTH_ALARM")             return Checked(settings.depth_alarm);
    if(var == "DEPTH_ALARM_MIN")         return String(settings.depth_alarm_min);
    if(var == "DEPTH_ALARM_RATE")        return Checked(settings.depth_alarm_rate);
    if(var == "DEPTH_ALARM_RATE_VALUE")  return String(settings.depth_alarm_rate_value);

    if(var == "AIS_ALARM")       return Checked(settings.ais_alarm);
    if(var == "AIS_ALARM_CPA")   return String(settings.ais_alarm_cpa);
    if(var == "AIS_ALARM_TCPA")  return String(settings.ais_alarm_tcpa);

    if(var == "PYPILOT_ALARM_NOCONNECTION")       return Checked(settings.pypilot_alarm_noconnection);
    if(var == "PYPILOT_ALARM_FAULT")       return Checked(settings.pypilot_alarm_fault);
    if(var == "PYPILOT_ALARM_NO_IMU")       return Checked(settings.pypilot_alarm_no_imu);
    if(var == "PYPILOT_ALARM_NO_MOTORCONTROLLER")       return Checked(settings.pypilot_alarm_no_motor_controller);
    if(var == "PYPILOT_ALARM_LOST_MODE")       return Checked(settings.pypilot_alarm_lost_mode);
    
    if(var == "VERSION")    return String(VERSION);
    return String();
}

void web_setup()
{
    ws.onEvent(onEvent);
    ws_data.onEvent(onEvent);
    server.addHandler(&ws);
    server.addHandler(&ws_data);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", String(), 0, processor);
    });

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
            String name = p->name(), value = p->value();
            //printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            if(name == "anchor_alarm")           settings.anchor_alarm = true;
            else if(name == "anchor_alarm_distance")  settings.anchor_alarm_distance = value.toInt();

            else if(name == "course_alarm")         settings.course_alarm = true;
            else if(name == "course_alarm_course")  settings.course_alarm_course = value.toInt();
            else if(name == "course_alarm_error")   settings.course_alarm_error = value.toInt();
            
            else if(name == "gps_speed_alarm")        settings.gps_speed_alarm = true;
            else if(name == "gps_min_speed_alarm_knots")  settings.gps_min_speed_alarm_knots = value.toInt();
            else if(name == "gps_max_speed_alarm_knots")  settings.gps_max_speed_alarm_knots = value.toInt();

            else if(name == "wind_speed_alarm")        settings.wind_speed_alarm = true;
            else if(name == "wind_min_speed_alarm_knots")  settings.wind_min_speed_alarm_knots = value.toInt();
            else if(name == "wind_max_speed_alarm_knots")  settings.wind_max_speed_alarm_knots = value.toInt();

            else if(name == "water_speed_alarm")        settings.water_speed_alarm = true;
            else if(name == "water_min_speed_alarm_knots")  settings.water_min_speed_alarm_knots = value.toInt();
            else if(name == "water_max_speed_alarm_knots")  settings.water_max_speed_alarm_knots = value.toInt();

            else if(name == "weather_alarm_pressure")      settings.weather_alarm_pressure = true;
            else if(name == "weather_alarm_min_pressure")  settings.weather_alarm_min_pressure = value.toInt();
            else if(name == "weather_alarm_pressure_rate")      settings.weather_alarm_pressure_rate = true;
            else if(name == "weather_alarm_pressure_rate_value")  settings.weather_alarm_pressure_rate_value = value.toInt();
            else if(name == "weather_alarm_lightning")      settings.weather_alarm_lightning = true;
            else if(name == "weather_alarm_lightning_distance")  settings.weather_alarm_lightning_distance = value.toInt();

            else if(name == "depth_alarm")      settings.depth_alarm = true;
            else if(name == "depth_alarm_min")  settings.depth_alarm_min = value.toInt();
            else if(name == "depth_alarm_rate")      settings.depth_alarm_rate = true;
            else if(name == "depth_alarm_rate_value")  settings.depth_alarm_rate_value = value.toInt();

            else if(name == "ais_alarm")      settings.ais_alarm = true;
            else if(name == "ais_alarm_cpa")  settings.ais_alarm_cpa = value.toInt();
            else if(name == "ais_alarm_tcpa")      settings.ais_alarm_tcpa = value.toInt();
            
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

    server.on("/alarms.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/alarms.html", String(), 0, processor_alarms);
    });

    server.on("/network", HTTP_POST, [](AsyncWebServerRequest *request) {
        //Serial.printf("post network\n");
        for (int i = 0; i < request->params(); i++) {
            AsyncWebParameter* p = request->getParam(i);
            String name = p->name(), value = p->value();
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
            String name = p->name(), value = p->value();
            //printf("POST[%s]: %s\n", name.c_str(), value.c_str());
            if     (name == "input_usb")   settings.input_usb = true;
            else if(name == "output_usb")  settings.output_usb = true;
            else if(name == "usb_baud_rate")  settings.usb_baud_rate = value.toInt();
            else if(name == "rs422_baud_rate")  settings.rs422_baud_rate = value.toInt();
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
            else if(name == "nmea_tcp_client_port") settings.nmea_client_port = value.toInt();
            else if(name == "nmea_tcp_server_port") settings.nmea_server_port = value.toInt();
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
        settings.powerdown = false;
        String enabled_pages = "";
        for(int i=0; i < display_pages.size(); i++)
            display_pages[i].enabled = false;
        for (int i = 0; i < request->params(); i++) {
            AsyncWebParameter* p = request->getParam(i);
            String name = p->name(), value = p->value();
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
                settings.contrast = min(max(value.toInt(), 0L), 50L);
            else if(p->name() == "backlight")
                settings.backlight = min(max(value.toInt(), 0L), 100L);
            else if(p->name() == "mirror")
                settings.mirror = true;
            else if(p->name() == "powerdown")
                settings.powerdown = true;
            else {
                for(int j=0; j < display_pages.size(); j++) {
                    page_info &page = display_pages[j];
                    if(String(page.name) == name)
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
        String data_type, data_range;
        if (request->hasParam("data_type"))
            data_type = request->getParam("data_type")->value();
        if (request->hasParam("data_range"))
            data_range = request->getParam("data_range")->value();

        int item = -1, range = -1;
        for(int i=0; i<DISPLAY_COUNT; i++)
            if(display_get_item_label((display_item_e)i) == data_type) {
                item = i;
                break;
            }

        for(int i=0; i<HISTORY_RANGE_COUNT; i++)
            if(history_get_label((history_range_e)i) == data_type) {
                range = i;
                break;
            }

        if(item >= 0 && range >= 0)
        ;//    request->send(200, "text/plain", history_get(item, range));
        else
            request->send(404);
    });

    server.serveStatic("/", SPIFFS, "/");

    server.begin();
    Serial.println("web server listening");
}

void web_poll()
{
    uint32_t t = millis();

    static uint32_t last_sock_update;
    if(t - last_sock_update < 200)
        return;
    last_sock_update = t;

    String s = jsonCurrent();
    if(s) {
        //Serial.println("ws: " + s);
        ws.textAll(s);
    }

    static uint32_t last_client_update;
    if(t - last_client_update < 2000)
        return;
    last_client_update = t;

    if(ws.count())
        ws.textAll(wireless_json_sensors());
    ws.cleanupClients();

    if(ws_data.count())
        ws_data.textAll(jsonDisplayData());
    ws_data.cleanupClients();
}
