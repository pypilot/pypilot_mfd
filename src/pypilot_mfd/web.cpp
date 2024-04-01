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
        Serial.printf("websocket got %s", data);
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
    char p = 'A';
    for(std::vector<page_info>::iterator it = display_pages.begin(); it!=display_pages.end(); it++) {
        pages += "<br><input type='checkbox' name='" + it->name + "' ";
        if(it->enabled)
            pages += "checked";
        pages += "><b>";
        pages += p;
        pages += "</b> " + it->description + "</input>";
        p++;
    }
}

String processor(const String& var)
{
    //Serial.println(var);
    if(var == "SSID")       return settings.ssid;
    if(var == "PSK")        return settings.psk;
    if(var == "CHANNEL")    return String(settings.channel);
    if(var == "INPUTUSB")   return String(settings.input_usb);
    if(var == "OUTPUTUSB")  return String(settings.output_usb);
    if(var == "INPUTWIFI")  return String(settings.input_wifi);
    if(var == "OUTPUTWIFI") return String(settings.output_wifi);
    if(var == "USB_BAUD_RATE") return String(settings.usb_baud_rate);
    if(var == "RS422_BAUD_RATE") return String(settings.rs422_baud_rate);
    if(var == "PYPILOT_ADDR") return(pypilot_discovered==2 ? settings.pypilot_addr : "not detected");
    if(var == "SIGNALK_ADDR") return(signalk_discovered==2 ? (settings.signalk_addr + ":" + String(settings.signalk_port)) : "not detected");
    if(var == "WIFIDATA")   return get_wifi_data_str();
    if(var == "NMEACLIENTADDR") return String(settings.nmea_client_addr);
    if(var == "NMEACLIENTPORT") return String(settings.nmea_client_port);
    if(var == "IPADDRESS") return WiFi.localIP().toString();    
    if(var == "NMEASERVERPORT") return String(settings.nmea_server_port);
    if(var == "USEFAHRENHEIT")  return String(settings.use_fahrenheit);
    if(var == "USEDEPTHFT")  return String(settings.use_depth_ft);
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
      Serial.printf("sent\n");
    });

    server.on("/network", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.printf("post network\n");
        for (int i = 0; i < request->params(); i++) {
            AsyncWebParameter* p = request->getParam(i);
            Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
        request->send(SPIFFS, "/index.html", String(), 0, processor);
        settings_store();
    });

    server.on("/data", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.printf("post data\n");
        for (int i = 0; i < request->params(); i++) {
            AsyncWebParameter* p = request->getParam(i);
            Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
        request->send(SPIFFS, "/index.html", String(), 0, processor);
        settings_store();
    });

    server.on("/display", HTTP_POST, [](AsyncWebServerRequest *request) {
        Serial.printf("post pages\n");
        for (int i = 0; i < request->params(); i++) {
            AsyncWebParameter* p = request->getParam(i);
            Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
            if(p->name() =="usefahrenheit")
                settings.use_fahrenheit = p->value();
            else if(p->name() =="usedepthft")
                settings.use_depth_ft = p->value();
            else if(p->name() =="latlonformat")
                settings.lat_lon_format = p->value();

            else {
                String enabled_pages = "";
                for(int i=0; i < display_pages.size(); i++) {
                    page_info &page = display_pages[i];
                    if(page.name == p->name())
                        page.enabled = p->value() == "true";
                    enabled_pages += p->value() ? "t" : "f";
                }
                settings.enabled_pages = enabled_pages;
            }
        }
        display_change_page(0);
        request->send(SPIFFS, "/index.html", String(), 0, processor);
        settings_store();
    });

    server.on("/wind.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/wind.js", String(), 0, processor);
    });

    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/styles.css");
    });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/favicon.ico");
    });

    server.begin();
    Serial.println("web server listening");
}

static uint32_t last_sock_update;
void web_poll()
{
    String s = jsonCurrent();
    if(s) {
        //Serial.println("ws: " + s);
        ws.textAll(s);
    }

    uint32_t t = millis();
    if(t - last_sock_update < 1000)
        return;
    last_sock_update = t;


    ws.cleanupClients();

    ws.textAll(jsonSensors());
}
