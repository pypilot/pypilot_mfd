/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "Arduino.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

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
#include "history.h"

//std::string token = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJkZXZpY2UiOiJweXBpbG90X21mZC0wMzgyMjQ2OTg4MCIsImlhdCI6MTcxMTY5MDA0MCwiZXhwIjoxNzQzMjQ3NjQwfQ.T-g3vkQMz5e6lbp9UGNEZgo6mEJex0i8eOeOUGUCGOI";
std::string access_url;
std::string ws_url;
std::string uid;

esp_websocket_client_handle_t ws_h;

float to_knots(float m_s) { return m_s * 1.94384; }
        float celcius(float kelvin) { kelvin - 273.15; }

static bool signalk_parse_value(const rapidjson::Value &value)
{
    if(!value.HasMember("path") || !value.HasMember("value"))
        return false;

    if(!value["path"].IsString() || value["value"].IsNull())
        return false;
    
    std::string path = value["path"].GetString();

    if(value.HasMember("timestamp")) {
        // TODO timestamp update history here
        //history_set_time(uint32_t date, int hour, int minute, float second);
    }
    
    const rapidjson::Value &val = value["value"];

    //printf("SIGNALK GOT PATH %s %s\n", path.c_str(), x.c_str());
    data_source_e s = WIFI_DATA;
    
    if(path == "navigation.attitude") {
        if(!val.IsObject() || !val.HasMember("pitch") || !val.HasMember("roll") || !val.HasMember("yaw"))
            return false;
        if(!val["pitch"].IsNumber() || !val["roll"].IsNumber() || !val["yaw"].IsNumber())
            return false;
        float pitch = val["pitch"].GetDouble();
        float roll = val["roll"].GetDouble();
        //float yaw = val["yaw"].GetDouble(); // should get this from magnetic heading

        display_data_update(PITCH, rad2deg(pitch), s);
        display_data_update(HEEL, rad2deg(roll), s);
        return true;
    }

    if(path == "navigation.position") {
        if(!val.IsObject() || !val.HasMember("latitude") || !val.HasMember("longitude"))
            return false;
        if(!val["latitude"].IsNumber() || !val["longitude"].IsNumber())
            return false;
        
        float lat = val["latitude"].GetDouble(), lon = val["longitude"].GetDouble();
        if(lat && lon) {
            display_data_update(LATITUDE, lat, s);
            display_data_update(LONGITUDE, lon, s);
        }
        return true;
    }

    if(!val.IsNumber())
        return true; // ignore message with key we dont handle

    float v = val.GetDouble();
        
    if(path == "environment.wind.speedApparent") display_data_update(WIND_SPEED, to_knots(v), s); else
    if(path == "environment.wind.angleApparent") display_data_update(WIND_DIRECTION, rad2deg(v), s); else
    if(path == "environment.wind.speedTrue") display_data_update(TRUE_WIND_SPEED, v, s);     else
    if(path == "environment.wind.angleTrue") display_data_update(TRUE_WIND_DIRECTION, v, s); else
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
    
    return true;
}

static bool signalk_parse_values(const rapidjson::Value &values)
{
    if(!values.IsArray())
        return false;
    for(int j=0; j<values.Size(); j++)
        if(!signalk_parse_value(values[j]))
            return false;
    return true;
}

static bool signalk_parse(const std::string line)
{
    //printf("signalk line %s\n", line.c_str());
    rapidjson::Document s;
    if(s.Parse(line.c_str()).HasParseError() || !s.IsObject() || !s.HasMember("updates"))
        return false;

    rapidjson::Value &updates = s["updates"];
    if(!updates.IsArray())
        return false;

    for(int i=0; i<updates.Size(); i++) {
        rapidjson::Value &update = updates[i];
        if(update.HasMember("values")) {
            if(!signalk_parse_values(updates["values"]))
                return false;
        } else if(update.HasMember("meta"))
            if(!signalk_parse_values(updates["meta"]))
                return false;
    }
    return true;
}

static std::string item_to_signalk_path(display_item_e item)
{
    switch(item) {
        case WIND_SPEED:          return "environment.wind.speedApparent";
        case WIND_DIRECTION:      return "environment.wind.angleApparent";
        case TRUE_WIND_SPEED:     return "environment.wind.speedTrue";
        case TRUE_WIND_DIRECTION: return "environment.wind.angleTrue";
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

    // unsubscribe all  (this could be a static string)
    rapidjson::StringBuffer us;
    rapidjson::Writer<rapidjson::StringBuffer> uw(us);
    uw.StartObject(); {
        uw.Key("context");
        uw.String("*");
        uw.Key("unsubscribe");
        uw.StartArray(); {
            uw.StartObject(); {
                uw.Key("path");
                uw.String("*");
            }
            uw.EndObject();
        }
        uw.EndArray();
    }
    uw.EndObject();

    esp_websocket_client_send_text(ws_h, us.GetString(), us.GetSize(), portMAX_DELAY);

    if(!settings.input_wifi)
        return;

    std::list<display_item_e> items;
    display_items(items);

    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> w(s);

    w.StartObject();
    w.Key("context");
    w.String("vessels.self");
    w.Key("subscribe");
    w.StartArray();
    for(std::list<display_item_e>::iterator it=items.begin(); it!=items.end(); it++) {
        std::string path = item_to_signalk_path(*it);
        if(path.empty())
            continue;
        w.StartObject();
        w.Key("path");
        w.String(path.c_str());
        w.Key("minPeriod");
        w.Int(1000); // milliseconds
        w.Key("format");
        w.String("delta");
        w.Key("policy");
        w.String("instant");
        w.EndObject();
    }
    w.EndArray();
    w.EndObject();

    esp_websocket_client_send_text(ws_h, s.GetString(), s.GetSize(), portMAX_DELAY);
}

static void signalk_disconnect()
{
    if(!ws_h)
        return;
    esp_websocket_client_stop(ws_h);
    esp_websocket_client_destroy(ws_h);
    ws_h = 0;
}

std::string random_number_string(int n)
{
    std::string ret;
    for(int i=0; i<n; i++)
        ret += int_to_str(random(10));
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
        client.begin(settings.signalk_addr.c_str(), settings.signalk_port, "/signalk/v1/access/requests");
        client.addHeader("Content-Type", "application/x-www-form-urlencoded");

        uid = "pypilot_mfd-" + random_number_string(11);
        String x = uid.c_str();
        int code = client.POST("clientId="+x+"&description=pypilot_mfd");
        if(code != 202 && code != 400) {
            printf("signalk http request failed to post %s:%d %d\n", settings.signalk_addr.c_str(), settings.signalk_port, code);
            settings.output_wifi = false;
        } else {
            String response = client.getString();
            //printf("signalk http response %s\n", response.c_str());
            rapidjson::Document s;
            if(s.Parse(response.c_str()).HasParseError() || !s.IsObject() || !s.HasMember("contents")) {
                printf("signalk failed to parse reply to post of signalk access requiest %s\n", response.c_str());
            } else {
                const rapidjson::Value &contents = s["contents"];
                if(!contents.IsObject() || !contents.HasMember("statusCode") || !contents["statusCode"].IsInt()) {
                    printf("signalk invalid response to access request %s\n", response.c_str());
                } else {
                    int code = contents["statusCode"].GetInt();
                    if((code == 202 || code == 400) && contents.HasMember("href")) {
                        access_url = contents["href"].GetString();
                        printf("signalk access url %s\n", access_url.c_str());
                    } else {
                        printf("signalk invalid status code or packet %s\n", response.c_str());
                    }
                }
            }
        }
        client.end();
        return;
    }

    HTTPClient client;
    String x = settings.signalk_addr.c_str();
    String y = access_url.c_str();
    client.begin(x, settings.signalk_port, y);
    int code = client.GET();
    if(code != 200) {
        printf("signalk http get access url failed %d\n", code);
        settings.output_wifi = false;
    } else {
        String response = client.getString(); // TODO change http client to not use arduino strings
        //printf("HTTP GET response %s %d\n", response.c_str(), code);
        rapidjson::Document s;
        if(s.Parse(response.c_str()).HasParseError() || !s.IsObject() || !s.HasMember("state")) {
            printf("signalk http response invalid %s\n", response.c_str());
            uid = "";
        } else {
            std::string state = s["state"].GetString();
            if(state == "COMPLETED") {
                if(s.HasMember("accessRequest")) {
                    const rapidjson::Value &access = s["accessRequest"];
                    if(access.HasMember("permission") && access.HasMember("token")) {
                        if(access["permission"].GetString() == "APPROVED") {
                            printf("signalk got new token!");
                            //store token, close signalk (to reconnect with token)
                            settings.signalk_token = access["token"].GetString();
                        }
                        signalk_disconnect();
                    } else
                        printf("invalid response to accessRequest: %s\n", response.c_str());
                } else {
                    //printf("FAILED REQUEST, tryihng agin\n");
                    access_url = "";
                }
            }
        }
    }
    client.end();
}

std::string websocket_buffer;
static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        printf("WEBSOCKET_EVENT_CONNECTED\n");
        signalk_subscribe();
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        printf("WEBSOCKET_EVENT_DISCONNECTED\n");
        signalk_disconnect();
        break;
    case WEBSOCKET_EVENT_DATA:
    {
        if(!settings.input_wifi)
            break;
        /*
        println("WEBSOCKET_EVENT_DATA");
        printf("Received opcode=%d", data->op_code);
        printf("Received=%.*s", data->data_len, (char *)data->data_ptr);
        printf("Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);
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
            if(!signalk_parse(websocket_buffer))
                printf("signalk failed to parse websocket data %s\n", websocket_buffer.c_str());
            websocket_buffer = "";
        } else if(websocket_buffer.length() > 4096)
            websocket_buffer = "";
        else if((websocket_buffer.length() == 0) == (d[0] == '{'))
            websocket_buffer += d;
    } break;
    case WEBSOCKET_EVENT_ERROR:
        printf_P(F("WEBSOCKET_EVENT_ERROR"));
        break;
    }
}

uint32_t signalk_connect_time;
std::string headerstring;
static void signalk_connect()
{
    uint32_t t0 = millis();
    if(t0-signalk_connect_time < 10000)
        return;
    signalk_connect_time = t0;

    HTTPClient client;
    client.begin(settings.signalk_addr.c_str(), settings.signalk_port, "/signalk");

    int httpResponseCode = client.GET();
    std::string payload;
    if (httpResponseCode>0) {
        printf("signalk HTTP Response code: %d", httpResponseCode);
        payload = client.getString().c_str();
        printf(" :%s\n", payload.c_str());
        client.end();
    } else {
        printf("signalk Error code: %d\n", httpResponseCode);
        client.end();
        return;
    }

    rapidjson::Document s;
    if(s.Parse(payload.c_str()).HasParseError()) {
        printf("signalk failed to parse %s\n");
        return;
    }

    if(!s.IsObject() || !s.HasMember("endpoints")) {
        printf("signalk payload hs no endpoints\n");
        return;
    }

    const rapidjson::Value &endpoints = s["endpoints"];
    if(!endpoints.IsObject() || !endpoints.HasMember("v1")) {
        printf("signalk no v1 endpoints\n");
        return;
    }

    const rapidjson::Value &v1 = s["v1"];
    if(!v1.IsObject() || !v1.HasMember("signalk-ws")) {
        printf("signalk v1 no signalk-ws url\n");
        return;
    }

    ws_url = v1["signlk-ws"].GetString();

    if(ws_url.empty()) {
        printf("signlk ws_url empty\n");
        return;
    }

    //ws_url += "?subscribe=none";
    esp_websocket_client_config_t ws_cfg = {0};
    ws_cfg.uri = ws_url.c_str();
    //ws_cfg.uri = "ws://10.10.10.1:3000/signalk/v1/stream";
    //ws_cfg.uri = "ws://10.10.10.66:12345/signalk/v1/stream";

    if(!settings.signalk_token.empty()) {
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

std::map<std::string, float> skupdates;
void signalk_send(std::string key, float value)
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
        if(settings.signalk_token.length() && !skupdates.empty()) {
            rapidjson::StringBuffer s;
            rapidjson::Writer<rapidjson::StringBuffer> w(s);
            w.StartObject(); {
                w.Key("updates");
                w.StartArray(); {
                    w.StartObject(); {
                        w.Key("$source");
                        w.String("pypilot_mfd");
                        w.Key("values");
                        w.StartArray();
                        for(std::map<std::string, float>::iterator it = skupdates.begin(); it != skupdates.end(); it++) {
                            w.StartObject();
                            w.Key("path");
                            w.String(it->first.c_str());
                            w.Key("value");
                            w.Double(it->second);
                            w.EndObject();
                        }
                        w.EndArray();
                    }
                    w.EndObject();
                }
                w.EndArray();
            }
            w.EndObject();

                //printf("SIGNALK try SEND?! %s\n", payload.c_str());
            esp_websocket_client_send_text(ws_h, s.GetString(), s.GetSize(), portMAX_DELAY);
            skupdates.clear();
        } else if(settings.output_wifi)
            request_access();
}
