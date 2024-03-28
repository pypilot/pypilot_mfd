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

#if 1
#include "esp_websocket_client.h"
#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "esp_transport_ssl.h"
#include "esp_transport_ws.h"
#endif


#include "signalk.h"
#include "display.h"
#include "zeroconf.h"
#include "utils.h"
#include "settings.h"

String token;
String access_url;
String uid;
String ws_url;

float to_knots(float m_s) { return m_s * 1.94384; }
        float celcius(float kelvin) { kelvin - 273.15; }

static void signalk_parse_value(JSONVar value)
{
    String path = value["path"];
    JSONVar val = value["value"];

    if(!path || !val)
        return;

    float v = (double)val;

    data_source_e s = WIFI_DATA;
        
    if(path == "environment.wind.speedApparent") display_data_update(WIND_SPEED, to_knots(v), s);       else
    if(path == "environment.wind.angleApparent") display_data_update(WIND_DIRECTION, rad2deg(v), s); else
//    if(path == "environment.wind.speedTrue") display_data_update(WIND_SPEED, v, s);     else
//    if(path == "environment.wind.angleTrue") display_data_update(WIND_DIRECTION, v, s); else
    if(path == "environment.outside.temperature") display_data_update(AIR_TEMPERATURE, celcius(v), s); else
    if(path == "environment.outside.pressure") display_data_update(BAROMETRIC_PRESSURE, v, s); else
    if(path == "environment.depth.belowSurface") display_data_update(DEPTH, v, s); else
    if(path == "navigation.courseOverGroundTrue") display_data_update(GPS_HEADING, rad2deg(v), s); else
    if(path == "navigation.speedOverGroundTrue") display_data_update(GPS_SPEED, to_knots(v), s); else
    if(path == "navigation.position") {
        double lat = val["latitude"], lon = val["longitude"];
        if(lat && lon) {
            display_data_update(LATITUDE, lat, s);
            display_data_update(LONGITUDE, lon, s);
        }
    } else
    if(path == "steering.rudderAngle") display_data_update(RUDDER_ANGLE, rad2deg(v), s); else
    if(path == "steering.autopilot.target.headingTrue") display_data_update(TRACK_BEARING, rad2deg(v), s); else
    if(path == "navigation.headingMagnetic") display_data_update(COMPASS_HEADING, rad2deg(v), s); else
    if(path == "navigation.attitude") {
//       navigation.attitude  {'pitch': 'pitch', 'roll': 'roll', 'yaw': 'heading_lowpass'},

    } else
    if(path == "navigation.rateOfTurn") display_data_update(RATE_OF_TURN, rad2deg(v), s); else
    if(path == "navigation.speedThroughWater") display_data_update(WATER_SPEED, to_knots(v), s); else
   // if(path == "navigation.leewayAngle") display_data_update(LEEWAY, rad2deg(v), s); else

    return;
}


static bool signalk_parse_line(String line)
{
    JSONVar input = JSON.parse(line);

    if(!input["updates"])
        return false;

    JSONVar updates = input["updates"];
    for(int i=0; i<updates.length(); i++) {
        JSONVar update = updates[i], values;
        if(update["values"])
            values = update["values"];
        else if(update["meta"])
            values = update["meta"];

        for(int j=0; j<values.length(); j++)
            signalk_parse_value(values[i]);
    }
    return true;
}


static void subscribe()
{    
    JSONVar subscription;
    subscription["path"] = "navigation.speedOverGround";
    subscription["minPeriod"] = 1000; // milliseconds
    subscription["format"] = "delta";
    subscription["policy"] = "instant";

    JSONVar subscriptions;
    subscriptions[0] = subscription;

    JSONVar msg;
    msg["context"] = "vessels.self";
    msg["subscribe"] = subscriptions;


}

bool ws_active = false;
esp_websocket_client_handle_t ws_h;
static void signalk_disconnect()
{
    if(!ws_active)
        return;
    esp_websocket_client_stop(ws_h);
    esp_websocket_client_destroy(ws_h);
    ws_active = false;
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
    /*
    // avoid requesting access too often
    uint32_t t0 = millis();
    static uint32_t request_time;
    if(t0 - request_time < 8000)
        return;
    request_time = t0;

    if(!access_url.length()) {
        WiFiClient wifi;
        HttpClient client = HttpClient(wifi, signalk_addr, signalk_port);
        uid = "pypilot_mfd-" + random_number_string(11);
        JSONVar data;
        data["clientId"] = uid;
        data["description"] = "pypilot_mfd";
        requests.post("/signalk/v1/access/requests", String(data))

        // read the status code and body of the response
        int statusCode = client.responseStatusCode();
        String response = client.responseBody();

        Serial.print("SIGNALK Status code: ");
        Serial.println(statusCode);
        Serial.print("SIGNALK Response: ");
        Serial.println(response);
        JSONVar = JSON.parse(response);
        int code = JSONVar["statusCode"];
        if(code == 202 || code == 400) {
           access_url = contents['href'];
        }
        return;
    }

    WiFiClient wifi;
    HttpClient client = HttpClient(wifi, signalk_addr, signalk_port);
    client.get(access_url);
    String response = client.responseBody();

    JSONVar contents = JSON.parse(response);
    if(contents["state"] == "COMPLETED") {
        JSONVar access = contents["accessRequest"];
        if(access) {
            JSONVar permission = access["permission"];
            if(permission == "APPROVED") {
                token = access["token"];
                //store token
                signalk_close();
        } else
            uid = "";           
    }
    */
}



static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        Serial.println("WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        Serial.println("WEBSOCKET_EVENT_DISCONNECTED");
        signalk_disconnect();
        break;
    case WEBSOCKET_EVENT_DATA:
    {
        Serial.println("WEBSOCKET_EVENT_DATA");
        Serial.printf("Received opcode=%d", data->op_code);
        Serial.printf("Received=%.*s", data->data_len, (char *)data->data_ptr);
        Serial.printf("Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);
        char d[data->data_len+1];
        memcpy(d, data->data_ptr, data->data_len);
        d[data->data_len] = '\0';
        String line(d);
        signalk_parse_line(line);
    } break;
    case WEBSOCKET_EVENT_ERROR:
        Serial.println("WEBSOCKET_EVENT_ERROR");
        break;
    }
}

uint32_t signalk_connect_time;
static void signalk_connect()
{
    uint32_t t0 = millis();
    if(t0-signalk_connect_time < 10000)
        return;
    signalk_connect_time = t0;


    #if 0

    HTTPClient http;
    String serverPath = signalk_addr + ":" + signalk_port "/signalk";
    // Your Domain name with URL path or IP address with path
    http.begin(serverPath.c_str());

    int httpResponseCode = http.GET();
    String payload;
    if (httpResponseCode>0) {
        Serial.print("signalk HTTP Response code: ");
        Serial.println(httpResponseCode);
        payload = http.getString();
        Serial.println(payload);
    } else {
        Serial.print("signalk Error code: ");
        Serial.println(httpResponseCode);
        http.end();
        return;
    }
    // Free resources
    http.end();

    JSONVar contents = JSON.parse(payload);
    ws_url = contents["endpoints"]["v1"]["signalk-ws"];
    #endif

    if(!ws_url)
        return;

    //ws_url += "?subscribe=none";

    String headerstring = "Authorization: JWT   " + token + "\r\n";

    esp_websocket_client_config_t ws_cfg;
    ws_cfg.uri = ws_url.c_str();
    //ws_cfg.headers = headerstring.c_str();
    // connect

    Serial.printf("Connecting to %s...\n", ws_cfg.uri);

    ws_h = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(ws_h, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)ws_h);

    esp_websocket_client_start(ws_h);
}

JSONVar updates;

void signalk_send(String key, float value)
{
    JSONVar update;
    update["path"] = key;
    update["value"] = value;
    updates[updates.length()] = update;
}

void signalk_poll()
{
    if(settings.wifi_data != SIGNALK) {
        signalk_disconnect();
        return;
    }

    if(!ws_active) {
        signalk_connect();
        return;
    }

    if (!esp_websocket_client_is_connected(ws_h))
        return;

    if(!token && settings.output_wifi)
        request_access();

    if(updates.length()) {
        String payload = JSON.stringify(updates);
        esp_websocket_client_send_text(ws_h, payload.c_str(), payload.length(), portMAX_DELAY);
        updates = JSONVar();
    }
}
