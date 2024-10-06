/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include "Arduino.h"
#include <lwip/sockets.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "zeroconf.h"
#include "pypilot_client.h"
#include "display.h"
#include "settings.h"
#include "utils.h"

#include <map>
#include <list>

struct pypilotClient
{
    pypilotClient();

    bool connect(std::string, int port=23322);
    void disconnect();
    bool connected() { return !!sock; }
    //bool receive(std::string &name, JSONVar &value);

    void set(const std::string name, const std::string value);

    void poll();

    //bool info(std::string name, JSONVar &info);
    
    //JSONVar list;

    std::string host;

    int      sock;
    std::string   sock_buffer;

    std::map<std::string, rapidjson::Document> map;
    std::map<std::string, float> watchlist;
    bool connecting;
    uint32_t connect_time;
};

pypilotClient::pypilotClient()
{
    sock = 0;
}

bool pypilotClient::connect(std::string h, int port)
{
    uint32_t t0 = millis();
    if(t0 - connect_time < 3000)
        return false;
    connect_time = t0;

    printf("pypilot conn %s %d\n", h.c_str(), port);
    host = h;
    const char *addr = host.c_str();

    sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    //inet_ntoa_r(dest_addr.sin_addr, addr, sizeof(addr) - 1);
                
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        printf("Unable to create socket: errno %d", errno);
        sock = 0;
        return false;
    }

    // Set socket to non-blocking mode
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
                
    int err = ::connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
    if (err != 0 && errno != EINPROGRESS) {
        printf("pypilot socket unable to connect: %d errno %d\n", sock, errno);
        disconnect();
        return false;
    }

    printf("pypilot client connected to %s:%d %d %d\n", addr, port, err, errno);
    connecting = true;
    return true;
}

void pypilotClient::disconnect()
{
    printf("pypilot client disconnect\n");
    close(sock);
    sock = 0;
    map.clear();
    sock_buffer = "";
}

/*
bool pypilotClient::receive(std::string &name, Json::Value &value)
{
    if(map.empty())
        return false;

    std::map<std::string, Json::Value>::iterator it = m_map.begin();
    name = it->first;
    value = it->second;
    m_map.erase(it);
    return true;
    }*/

void pypilotClient::set(const std::string name, const std::string json)
{
    if(!connected())
        return;

    std::string str = name + "=" + json + "\n";
    int ret = send(sock, str.c_str(), str.length(), 0);
    printf("send %s %d %d\n", str.c_str(), str.length(), ret);
    if(ret < 0) {
        if(errno == EAGAIN)
            printf("pypilot socket couldnt take data\n");
        else {
            printf("Set error errno %d\n", errno);
            disconnect();

        }
    }
}

/*
bool pypilotClient::info(std::string name, JSONVar &info)
{
    info = m_list[name.c_str()];
    return !info.isNull();
}*/

/*
void pypilotClient::update_watchlist(std::map<std::string, float> &watchlist_)
{
    JSONVar request;
    // watch new keys we weren't watching
    for(std::map<std::string, float>::iterator it = watchlist_.begin(); it != watchlist_.end(); it++)
        if(m_watchlist_.find(it->first) == m_watchlist_.end())
            request[it->first] = it->second;

    // unwatch old keys we don't need
    for(std::map<std::string, float>::iterator it = watchlist.begin(); it != watchlist.end(); it++)
        if(watchlist.find(it->first) == watchlist.end())
            request[it->first] = false;

    if(request.size())
        set("watch", request);
    watchlist = watchlist_;
    }*/

void pypilotClient::poll()
{
    if(!sock)
        return;

    if(connecting) {
        int ret = send(sock, 0, 0, 0);
        printf("pypilot client poll send0 %d\n", ret);
        if(ret < 0) {
            if(errno == EAGAIN)
                return;
            printf("pypilot client closed on receive %d\n", errno);
            disconnect();
            return;
        }
        printf("pypilot client ready\n");
        // return 0 means ready
        map.clear();
        sock_buffer = "";

        //watch("values");
        if(watchlist.size()) {
            rapidjson::StringBuffer s;
            rapidjson::Writer<rapidjson::StringBuffer> writer(s);
            writer.SetMaxDecimalPlaces(6);
            writer.StartObject();
            for(std::map<std::string, float>::iterator it = watchlist.begin();
                it != watchlist.end(); it++) {
                writer.Key(it->first.c_str());
                writer.Double(it->second);
            }
            writer.EndObject();
            set("watch", s.GetString());
        }
        connecting = false;
    }

    for(;;) {
        char buf[1024];
        int ret = recv(sock, buf, sizeof buf-1, 0);
        //printf("pypilot client recv %d %d\n", ret, errno);
        if(ret < 0) {
            if(errno == EAGAIN)
                return;
            printf("pypilot client socket recv failed %d\n", errno);
            disconnect();
            if(errno == EBADF) {
                printf("EBADF received: resetting esp\n");
                //ESP.restart();
            }
            return;
        }

        if(ret >= sizeof buf) {
            printf("AHAH!!!  ret too big %d\n", ret);
            return;
        }
        
        buf[ret] = 0;
        sock_buffer += buf;
        // overflow and reset connection at 64k in buffer
        if(sock_buffer.length() >= 1024*64) {
            printf("pypilot client input buffer overflow!\n");
            disconnect();
            return;
        }
        
        int start = 0, line_end;
        for(;;) {
            line_end = sock_buffer.find("\n", start);
            if(line_end <= 0)
                break;

            int c = sock_buffer.find('=', start);
            if(c < 0) {
                printf("pypilot client: Error parsing - %s\n", sock_buffer.c_str());
                continue;
            }
            
            std::string key = sock_buffer.substr(start, c-start);
            std::string value = sock_buffer.substr(c+1, line_end-c-1);
            start = line_end + 1;

            //printf("pypilot client %s = %s\n", key.c_str(), json_line.c_str());
            rapidjson::Document &d = map[key];
            if(d.Parse(value.c_str()).HasParseError()) {
                printf("pypilot client: Error parsing message - %s=%s", key.c_str(), value.c_str());
                map.erase(key);
                continue;
            }

            if(key == "values") {
                // for now do not parse values
            }
        }

          //      printf("RECV0 %s %d\n", sock_buffer.c_str(),start);

        sock_buffer = sock_buffer.substr(start);
        //printf("RECV1 %s\n", sock_buffer.c_str());
    }
}

static pypilotClient pypilot_client;
static uint32_t strobe_time;

void pypilot_client_strobe() {
    strobe_time = millis();
}

void pypilot_client_poll() {
    if(millis() - strobe_time > 5000 || settings.pypilot_addr != pypilot_client.host) {
        if(pypilot_client.connected())
            pypilot_client.disconnect();
        pypilot_client.host = settings.pypilot_addr;
    } else if(pypilot_client.connected()) {
        pypilot_client.poll();
        display_data_update(PYPILOT, 0, WIFI_DATA);
    } else
        pypilot_client.connect(settings.pypilot_addr);
}

void pypilot_watch(std::string key)
{
    pypilot_client.watchlist[key] = 1.f; // 1 second
}

std::string pypilot_client_value(std::string key, int digits) {
    if(pypilot_client.map.find(key) == pypilot_client.map.end())
        return "N/A";

    rapidjson::Document &d = pypilot_client.map[key];
    if(d.IsString())
        return d.GetString();

    if(d.IsNumber())
        return float_to_str(d.GetDouble(), digits);

    if(d.IsBool())
        return d.GetBool() ? "true" : "false";

    printf("unhandled pypilot client value %s\n", key.c_str());
    return "";
}
