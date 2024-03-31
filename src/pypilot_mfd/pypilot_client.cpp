/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <Arduino_JSON.h>

#include <lwip/sockets.h>

#include "zeroconf.h"
#include "pypilot_client.h"
#include "settings.h"


#include <map>
#include <list>

struct pypilotClient
{
    pypilotClient();

    bool connect(String, int port=23322);
    void disconnect();
    bool connected() { return !!sock; }
    //bool receive(String &name, JSONVar &value);

    void set(String name, JSONVar &value);

    void poll();

    //bool info(String name, JSONVar &info);
    
    //JSONVar list;

    String host;

    int      sock;
    String   sock_buffer;

    std::map<String, JSONVar> map;
    std::map<String, double> watchlist;
    bool connecting;
};

pypilotClient::pypilotClient()
{
    sock = 0;
}

bool pypilotClient::connect(String h, int port)
{
    host = h;
    const char *addr = host.c_str();

    sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    //inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
                
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        Serial.printf("Unable to create socket: errno %d", errno);
        sock = 0;
        return false;
    }

    // Set socket to non-blocking mode
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
                
    int err = ::connect(sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
    if (err != 0 || errno != EINPROGRESS) {
        Serial.printf("Socket unable to connect: errno %d\n", errno);
        disconnect();
    }

    Serial.printf("pypilot client connected to %s:%d %d %d\n", addr, port, err, errno);
    connecting = true;
    return true;
}

void pypilotClient::disconnect()
{
    close(sock);
    map.clear();
    sock_buffer = "";
}

/*
bool pypilotClient::receive(String &name, Json::Value &value)
{
    if(map.empty())
        return false;

    std::map<String, Json::Value>::iterator it = m_map.begin();
    name = it->first;
    value = it->second;
    m_map.erase(it);
    return true;
    }*/

void pypilotClient::set(String name, JSONVar &value)
{
    if(!connected())
        return;

    String str = name + "=" + JSON.stringify(value);
    int ret = send(sock, str.c_str(), str.length(), 0);
    if(ret < 0) {
        if(errno == EAGAIN)
            printf("pypilot socket couldnt take data\n");
        else
            disconnect();
    }
}

/*
bool pypilotClient::info(String name, JSONVar &info)
{
    info = m_list[name.c_str()];
    return !info.isNull();
}*/

/*
void pypilotClient::update_watchlist(std::map<String, double> &watchlist_)
{
    JSONVar request;
    // watch new keys we weren't watching
    for(std::map<String, double>::iterator it = watchlist_.begin(); it != watchlist_.end(); it++)
        if(m_watchlist_.find(it->first) == m_watchlist_.end())
            request[it->first] = it->second;

    // unwatch old keys we don't need
    for(std::map<String, double>::iterator it = watchlist.begin(); it != watchlist.end(); it++)
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
        int ret = recv(sock, 0, 0, 0);
        if(ret < 0) {
            if(errno == EAGAIN)
                return;
            printf("pypilot client closed on receive\n");
            disconnect();
            return;
        }
        // return 0 means ready
        map.clear();
        sock_buffer = "";

        //watch("values");
        if(watchlist.size()) {
            JSONVar request;
            for(std::map<String, double>::iterator it = watchlist.begin();
                it != watchlist.end(); it++)
                request[it->first] = it->second;
            set("watch", request);
        }
        connecting = false;
    }

    for(;;) {
        char buf[1024];
        int ret = recv(sock, buf, sizeof buf-1, 0);
        if(ret < 0) {
            if(errno == EAGAIN)
                return;
            printf("pypilot client socket recv failed %d\n", errno);
            disconnect();
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
        
        int start, line_end = 0;
        for(;;) {
            start = line_end;
            line_end = sock_buffer.indexOf("\n", start);
            if(line_end <= 0)
                break;
            String line = sock_buffer.substring(start, line_end);
            int c = line.indexOf('=', start);
            if(c < 0) {
                printf("pypilot client: Error parsing - %s", line.c_str());
                continue;
            }
            
            String key = line.substring(start, c);
            String json_line = line.substring(c+1);
            JSONVar value = JSON.parse(json_line);
            if(value == null) {
                printf("pypilot cleint: Error parsing message - %s=%s", key.c_str(), json_line.c_str());
                continue;
            }
            if(key == "values") {

            } else
                map[key] = value;
        }
        sock_buffer = sock_buffer.substring(line_end+1);
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
    } else if(pypilot_client.connected())
        pypilot_client.poll();
    else
        pypilot_client.connect(settings.pypilot_addr);
}

String pypilot_watch(String key)
{
    pypilot_client.watchlist[key] = 1; // one second for now
}

String pypilot_client_value(String key) {
    if(pypilot_client.map.find(key) == pypilot_client.map.end())
        return "N/A";
    return pypilot_client.map[key];
}
