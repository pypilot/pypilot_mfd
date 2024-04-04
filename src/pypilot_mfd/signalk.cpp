/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "Arduino.h"
#include <Arduino_JSON.h>

#include <HTTPClient.h>

#include "esp_websocket_client.h"
#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "esp_transport_ssl.h"
#include "esp_transport_ws.h"


#include "signalk.h"
#include "display.h"
#include "zeroconf.h"
#include "utils.h"
#include "settings.h"

//String token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJkZXZpY2UiOiJweXBpbG90X21mZC0wMzgyMjQ2OTg4MCIsImlhdCI6MTcxMTY5MDA0MCwiZXhwIjoxNzQzMjQ3NjQwfQ.T-g3vkQMz5e6lbp9UGNEZgo6mEJex0i8eOeOUGUCGOI";
String access_url;
String ws_url;
String uid;

esp_websocket_client_handle_t ws_h;

float to_knots(float m_s) { return m_s * 1.94384; }
        float celcius(float kelvin) { kelvin - 273.15; }

static void signalk_parse_value(JSONVar value)
{
    if(!value.hasOwnProperty("path") || !value.hasOwnProperty("value"))
        return;

    String path = value["path"];
    JSONVar val = value["value"];

    if(val == null)
        return;

    //const String x = val;
    //printf("SIGNALK GOT PATH %s %s\n", path.c_str(), x.c_str());


    data_source_e s = WIFI_DATA;
    
    if(path == "navigation.attitude") {
        double pitch = val["pitch"];
        double roll = val["roll"];
        double yaw = val["yaw"];

        display_data_update(PITCH, rad2deg(pitch), s);
        display_data_update(HEEL, rad2deg(roll), s);
        return;
    }

    if(path == "navigation.position") {
        double lat = val["latitude"], lon = val["longitude"];
        if(lat && lon) {
            display_data_update(LATITUDE, lat, s);
            display_data_update(LONGITUDE, lon, s);
        }
        return;
    }
    
    float v = (double)val;
        
    if(path == "environment.wind.speedApparent") display_data_update(WIND_SPEED, to_knots(v), s); else
    if(path == "environment.wind.angleApparent") display_data_update(WIND_DIRECTION, rad2deg(v), s); else
//    if(path == "environment.wind.speedTrue") display_data_update(WIND_SPEED, v, s);     else
//    if(path == "environment.wind.angleTrue") display_data_update(WIND_DIRECTION, v, s); else
    if(path == "environment.outside.temperature") display_data_update(AIR_TEMPERATURE, celcius(v), s); else
    if(path == "environment.outside.pressure") display_data_update(BAROMETRIC_PRESSURE, v, s); else
    if(path == "environment.depth.belowSurface") display_data_update(DEPTH, v, s); else
    if(path == "navigation.courseOverGroundTrue") display_data_update(GPS_HEADING, rad2deg(v), s); else
    if(path == "navigation.speedOverGroundTrue") display_data_update(GPS_SPEED, to_knots(v), s); else
    if(path == "steering.rudderAngle") display_data_update(RUDDER_ANGLE, rad2deg(v), s); else
    if(path == "steering.autopilot.target.headingTrue") route_info.target_bearing = rad2deg(v), display_data_update(ROUTE_INFO, 0, s); else
    if(path == "navigation.headingMagnetic") display_data_update(COMPASS_HEADING, rad2deg(v), s); else
    if(path == "navigation.rateOfTurn") display_data_update(RATE_OF_TURN, rad2deg(v), s); else
    if(path == "navigation.speedThroughWater") display_data_update(WATER_SPEED, to_knots(v), s); else
   // if(path == "navigation.leewayAngle") display_data_update(LEEWAY, rad2deg(v), s); else
        ;
    
    return;
}

static bool signalk_parse(String line)
{
    //printf("signalk line %s\n", line.c_str());
    JSONVar input = JSON.parse(line);
    if(!input.hasOwnProperty("updates"))
        return false;

    JSONVar updates = input["updates"];
    for(int i=0; i<updates.length(); i++) {
        JSONVar update = updates[i], values;
        if(update.hasOwnProperty("values"))
            values = update["values"];
        else if(update.hasOwnProperty("meta"))
            values = update["meta"];
        for(int j=0; j<values.length(); j++)
            signalk_parse_value(values[j]);
    }
    return true;
}

static String item_to_signalk_path(display_item_e item)
{
    switch(item) {
        case WIND_SPEED:          return "environment.wind.speedApparent";
        case WIND_DIRECTION:      return "environment.wind.angleApparent";
        case AIR_TEMPERATURE:     return "environment.outside.temperature";
        case BAROMETRIC_PRESSURE: return "environment.outside.pressure";
        case DEPTH:               return "environment.depth.belowSurface";
        case GPS_SPEED:           return "navigation.speedOverGroundTrue";
        case RUDDER_ANGLE:        return "steering.rudderAngle";
        case ROUTE_INFO:          return "steering.autopilot.target.headingTrue"; // extend to multiple  keys?!
        case COMPASS_HEADING:     return "navigation.headingMagnetic";
        case RATE_OF_TURN:        return "navigation.rateOfTurn";
        case WATER_SPEED:         return "navigation.speedThroughWater";
    }
    return "";
}

void signalk_subscribe()
{
    if (!esp_websocket_client_is_connected(ws_h))
        return;

    // unsubscribe all
    JSONVar unsubscribe;
    unsubscribe["context"] = "*";
    JSONVar pathall;
    pathall["path"] = "*";
    JSONVar pathsall;
    pathsall[0] = pathall;
    unsubscribe["unsubscribe"] = pathall;

    String payload = JSON.stringify(unsubscribe);
    esp_websocket_client_send_text(ws_h, payload.c_str(), payload.length(), portMAX_DELAY);

    if(!settings.input_wifi)
        return;

    std::list<display_item_e> items;
    display_items(items);

    int i=0;
    JSONVar subscriptions;
    for(std::list<display_item_e>::iterator it=items.begin(); it!=items.end(); it++) {
        JSONVar subscription;
        String path = item_to_signalk_path(*it);
        if(!path)
            continue;
        subscription["path"] = path;
        subscription["minPeriod"] = 1000; // milliseconds
        subscription["format"] = "delta";
        subscription["policy"] = "instant";

        subscriptions[i++] = subscription;
    }

    JSONVar msg;
    msg["context"] = "vessels.self";
    msg["subscribe"] = subscriptions;

    payload = JSON.stringify(msg);
    esp_websocket_client_send_text(ws_h, payload.c_str(), payload.length(), portMAX_DELAY);
}

static void signalk_disconnect()
{
    if(!ws_h)
        return;
    esp_websocket_client_stop(ws_h);
    esp_websocket_client_destroy(ws_h);
    ws_h = 0;
}

String random_number_string(int n)
{
    String ret;
    for(int i=0; i<n; i++)
        ret += String(random(10));
    return ret;
}

static void request_access()
{
    // avoid requesting access too often
    uint32_t t0 = millis();
    static uint32_t request_time;
    if(t0 - request_time < 5000)
        return;
    request_time = t0;

    //printf("signalk request access\n");

    if(!access_url.length()) {
        HTTPClient client;
        client.begin(settings.signalk_addr, settings.signalk_port, "/signalk/v1/access/requests");
        client.addHeader("Content-Type", "application/x-www-form-urlencoded");

        uid = "pypilot_mfd-" + random_number_string(11);
        int code = client.POST("clientId="+uid+"&description=pypilot_mfd");
        if(code != 202 && code != 400) {
            printf("signalk http request failed to post %s:%d %d\n", settings.signalk_addr.c_str(), settings.signalk_port, code);
            settings.output_wifi = false;
        } else {
            String response = client.getString();
            //printf("signalk http response %s\n", response.c_str());
            JSONVar contents = JSON.parse(response);
            int code = contents["statusCode"];
            if((code == 202 || code == 400) && contents.hasOwnProperty("href")) {
                String url = contents["href"];
                access_url = url;
            }
        }
        client.end();
        return;
    }

    HTTPClient client;
    client.begin(settings.signalk_addr, settings.signalk_port, access_url);
    int code = client.GET();
    if(code != 200) {
        printf("signalk http get access url failed %d\n", code);
        settings.output_wifi = false;
    } else {
        String response = client.getString();
        //printf("HTTP GET response %s %d\n", response.c_str(), code);

        JSONVar contents = JSON.parse(response);
        if(contents.hasOwnProperty("state")) {
            String state = contents["state"];
            if(state == "COMPLETED") {
                if(contents.hasOwnProperty("accessRequest")) {
                    JSONVar access = contents["accessRequest"];
                    String permission = access["permission"];
                    if(permission == "APPROVED") {
                        printf("signalk got new token!");
                        //store token, close signalk (to reconnect with token)
                        settings.signalk_token = JSON.stringify(access["token"]);
                        signalk_disconnect();
                    }
                } else {
                    //printf("FAILED REQUEST, tryihng agin\n");
                    access_url = "";
                }
            }
        } else
            uid = "";           
    }
    client.end();
}

String websocket_buffer;
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        Serial.println("WEBSOCKET_EVENT_CONNECTED");
        signalk_subscribe();
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        Serial.println("WEBSOCKET_EVENT_DISCONNECTED");
        signalk_disconnect();
        break;
    case WEBSOCKET_EVENT_DATA:
    {
        if(!settings.input_wifi)
            break;
        /*
        Serial.println("WEBSOCKET_EVENT_DATA");
        Serial.printf("Received opcode=%d", data->op_code);
        Serial.printf("Received=%.*s", data->data_len, (char *)data->data_ptr);
        Serial.printf("Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);
        */
        char d[data->data_len+1];
        memcpy(d, data->data_ptr, data->data_len);
        d[data->data_len] = '\0';
        websocket_buffer += d;
        int c = 0;
        for(int i=0; i<websocket_buffer.length(); i++) {
            if(websocket_buffer[i] == '{')
                c++;
            else if(websocket_buffer[i] == '}')
                c--;
        }
        if(c == 0) {
            signalk_parse(websocket_buffer);
            websocket_buffer = "";
        } else if(websocket_buffer.length() > 4096)
            websocket_buffer = "";
        else if((websocket_buffer.length() == 0) == (d[0] == '{'))
            websocket_buffer += d;
    } break;
    case WEBSOCKET_EVENT_ERROR:
        Serial.println("WEBSOCKET_EVENT_ERROR");
        break;
    }
}

uint32_t signalk_connect_time;
String headerstring;
static void signalk_connect()
{
    uint32_t t0 = millis();
    if(t0-signalk_connect_time < 10000)
        return;
    signalk_connect_time = t0;

    HTTPClient client;
    client.begin(settings.signalk_addr, settings.signalk_port, "/signalk");

    int httpResponseCode = client.GET();
    String payload;
    if (httpResponseCode>0) {
        Serial.print("signalk HTTP Response code: ");
        Serial.print(httpResponseCode);
        payload = client.getString();
        Serial.print(" :");
        Serial.println(payload);
        client.end();
    } else {
        Serial.print("signalk Error code: ");
        Serial.println(httpResponseCode);
        client.end();
        return;
    }

    JSONVar contents = JSON.parse(payload);
    const String jsonurl = contents["endpoints"]["v1"]["signalk-ws"];
    ws_url = jsonurl;

    if(!ws_url)
        return;

    //ws_url += "?subscribe=none";
    esp_websocket_client_config_t ws_cfg = {0};
    ws_cfg.uri = ws_url.c_str();
    //ws_cfg.uri = "ws://10.10.10.1:3000/signalk/v1/stream";
    //ws_cfg.uri = "ws://10.10.10.66:12345/signalk/v1/stream";

    if(settings.signalk_token) {
        headerstring = "Authorization: JWT ";
        headerstring += settings.signalk_token;
        headerstring += "\r\n";
        ws_cfg.headers = (char*)headerstring.c_str();
    }
    
    // connect
    printf("Connecting to %s...\n", ws_cfg.uri);

    ws_h = esp_websocket_client_init(&ws_cfg);
    if(!ws_h) {
        printf("signalk websock failed to connect\n");
        return;
    }
    esp_websocket_register_events(ws_h, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)ws_h);
    esp_websocket_client_start(ws_h);
    printf("signalk web sock complete\n");
}

std::map<String, float> skupdates;
void signalk_send(String key, float value)
{
    skupdates[key] = value;
}

void signalk_poll()
{
    if(settings.wifi_data != SIGNALK || !(settings.input_wifi || settings.output_wifi)) {
        signalk_disconnect();
        return;
    }

    if(!ws_h) {
        signalk_connect();
        return;
    }

    if (!esp_websocket_client_is_connected(ws_h))
        return;

    if(settings.output_wifi)
        if(settings.signalk_token.length()) {
            JSONVar upd;
            int i=0;
            for(std::map<String, float>::iterator it = skupdates.begin(); it != skupdates.end(); it++) {
                JSONVar update;
                update["path"] = it->first;
                update["value"] = it->second;
                upd[i++] = update;
            }
            if(i) {
                JSONVar upd2;
                upd2["$source"] = "pypilot_mfd";
                upd2["values"] = upd;
                JSONVar upd1;
                upd1[0] = upd2;
                JSONVar updates;
                updates["updates"] = upd1;
                String payload = JSON.stringify(updates);
                //printf("SIGNALK try SEND?! %s\n", payload.c_str());
                esp_websocket_client_send_text(ws_h, payload.c_str(), payload.length(), portMAX_DELAY);
                skupdates.clear();
            }
        } else if(settings.output_wifi)
            request_access();
}
