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
#include "display.h"
#include "utils.h"
#include "zeroconf.h"
#include "web.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

wind_position str2position(String p) {
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
        if(message == "scan") ; // do scan put all sensors on same channel

        else {
            JSONVar input = JSON.parse(message);
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
        }
    }
}

String position_str(wind_position p)
{
    switch(p) {
       case PRIMARY:   return "Primary";
       case SECONDARY: return "Secondary";
       case PORT:      return "Port";
       case STARBOARD: return "Starboard";
       case IGNORED:   return "Ignored";
    }
    return "Invalid";
}

static String jsonSensors() {
    uint32_t t = millis();
    JSONVar sensors;
    for(std::map<uint64_t, wind_transmitter_t>::iterator i = wind_transmitters.begin(); i != wind_transmitters.end(); i++) {
        JSONVar jwt;
        wind_transmitter_t &wt = i->second;

        jwt["position"] = position_str(wt.position);
        jwt["offset"] = wt.offset;
        jwt["dir"] = wt.dir;
        jwt["knots"] = wt.knots;
        jwt["dt"] = t - wt.t;

        sensors[mac_int_to_str(i->first)] = jwt;
    }
    return JSON.stringify(sensors);
}

static String jsonCurrent()
{
    JSONVar j;
    j["direction"] = lpdir;
    j["knots"] = knots;
    return JSON.stringify(j);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            ws.textAll(jsonSensors());
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
    if(var == "SSID")       return settings.ssid;
    if(var == "PSK")        return settings.psk;
    if(var == "CHANNEL")    return String(settings.channel);
    if(var == "INPUTUSB")   return Checked(settings.input_usb);
    if(var == "OUTPUTUSB")  return Checked(settings.output_usb);
    if(var == "INPUTWIFI")  return Checked(settings.input_wifi);
    if(var == "OUTPUTWIFI") return Checked(settings.output_wifi);
    if(var == "USB_BAUD_RATE") return String(settings.usb_baud_rate);
    if(var == "RS422_BAUD_RATE") return String(settings.rs422_baud_rate);
    if(var == "PYPILOT_ADDR") return(pypilot_discovered==2 ? settings.pypilot_addr : "not detected");
    if(var == "SIGNALK_ADDR") return(signalk_discovered==2 ? (settings.signalk_addr + ":" + String(settings.signalk_port)) : "not detected");
    if(var == "WIFIDATA")   return get_wifi_data_str();
    if(var == "NMEACLIENTADDR") return String(settings.nmea_client_addr);
    if(var == "NMEACLIENTPORT") return String(settings.nmea_client_port);
    if(var == "IPADDRESS") return WiFi.localIP().toString();    
    if(var == "NMEASERVERPORT") return String(settings.nmea_server_port);
    if(var == "USE360")  return Checked(settings.use_360);
    if(var == "USEFAHRENHEIT")  return Checked(settings.use_fahrenheit);
    if(var == "USEDEPTHFT")  return Checked(settings.use_depth_ft);
    if(var == "LATLONFORMAT")  return settings.lat_lon_format;
    if(var == "CONTRAST")  return String(settings.contrast);
    if(var == "DISPLAYPAGES") return get_display_pages();
    if(var == "VERSION")    return String(VERSION);
    return String();
}

void web_setup()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", String(), 0, processor);
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

    server.on("/data", HTTP_POST, [](AsyncWebServerRequest *request) {
        //printf("post data %d\n", request->params());
        settings.input_usb = settings.output_usb = false;
        settings.input_wifi = settings.output_wifi = false;
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
        settings.use_360 = settings.use_fahrenheit = settings.use_depth_ft = false;
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
            else if(p->name() == "usedepthft")
                settings.use_depth_ft = true;
            else if(p->name() == "lat_lon_format")
                settings.lat_lon_format = value;
            else if(p->name() == "contrast")
                settings.contrast = value.toInt();
            else if(p->name() == "backlight")
                settings.backlight = value.toInt();
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
        display_change_page(0);
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


    ws.cleanupClients();

    ws.textAll(jsonSensors());
}
