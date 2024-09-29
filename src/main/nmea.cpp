/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <WiFi.h>

#include <lwip/sockets.h>
#include <errno.h>

#include "display.h"
#include "nmea.h"
#include "zeroconf.h"
#include "ais.h"
#include "settings.h"
#include "history.h"

static uint8_t checksum(const char *buf, int len=-1)
{
    uint8_t cksum = 0;
    int i=0;
    while(*buf) {
        cksum ^= *buf++;
        if(++i == len)
            break;
    }
    return cksum;
}

static bool prefix(const char *line, const char *prefix)
{
    return line[3] == prefix[0] && line[4] == prefix[1] && line[5] == prefix[2];
}

static const char *comma(const char *start, int count)
{
    while(count) {
        start++;
        if(*start == ',')
            count--;
        if(!*start)
            return 0;
    }
    return start;
}

float convert_decimal_ll(float decll)
{
    float degrees;
    float minutes = modff(decll/100, &degrees)*100;
    return degrees + minutes / 60;
}

bool nmea_parse_line(const char *line, data_source_e source)
{
    //printf("nmea parse line %d %s\n", source, line);
    // check checksum
    int len=strlen(line);
    if(len < 10 || len > 180)
        return false;

    // ensure line starts with $ or !
    if(line[0] != '$' && line[0] != '!')
        return false;

    // find the '*' that separates data from checksum
    int indstar=len-1;
    while(line[indstar]!='*')
        if(--indstar == 0)
            return false;

    int cksum = strtol(line+indstar+1, 0, 16);
    if(cksum != checksum(line+1, indstar-1))
        return false;

    if(line[0] == '!') // probably AIS
        return ais_parse_line(line, source);

    const char *c1 = line+6;
    if(prefix(line, "MWV")) {
        float dir, spd;
        char ref;
        if(sscanf(c1, ",%f,%c,%f,N", &dir, &ref, &spd) != 3)
            return false;

        if(ref == 'R') {
            display_data_update(WIND_ANGLE, dir, source);
            display_data_update(WIND_SPEED, spd, source);
            return true;
        } if(ref != 'T') {
            display_data_update(TRUE_WIND_ANGLE, dir, source);
            display_data_update(TRUE_WIND_SPEED, spd, source);
            return true;
        }
        return false;
    }

    if(prefix(line, "MWD")) {
        float dir, spd;
        char ref;
        if(sscanf(c1, ",%f,%c,", &dir, &ref) != 3)
            return false;

        const char *c5 = comma(c1, 4);
        if(!c5) return false;
        if(sscanf(c5, ",%f,N", &spd) != 1)
            return false;
        
        display_data_update(TRUE_WIND_ANGLE, dir, source);
        display_data_update(TRUE_WIND_SPEED, spd, source);
        return true;
    }

    if(prefix(line, "VWR") || prefix(line, "VWT")) {
        float dir, spd;
        char ref;
        if(sscanf(c1, ",%f,%c,", &dir, &ref) != 3)
            return false;

        if(ref == 'L')
            dir = -dir;
        else if(ref != 'R')
            return false;

        const char *c5 = comma(c1, 4);
        if(!c5) return false;
        if(sscanf(c5, ",%f,N", &spd) != 1)
            return false;

        if(line[5] == 'T') {
            display_data_update(WIND_ANGLE, dir, source);
            display_data_update(WIND_SPEED, spd, source);
        } else {
            display_data_update(TRUE_WIND_ANGLE, dir, source);
            display_data_update(TRUE_WIND_SPEED, spd, source);
        }
        return true;
    }
    
    if(prefix(line, "MDA")) {
        float pressure, temperature;
        const char *c3 = comma(c1, 2); // skip ahead 2 commas
        if(!c3) return false;

        char unit;
        if(sscanf(c3, ",%f,%c", &pressure, &unit) != 2 || unit != 'B')
            return false;
        display_data_update(BAROMETRIC_PRESSURE, pressure, source);

        const char *c5 = comma(c3, 2);
        if(!c5) return false;
        if(sscanf(c5, ",%f,%c", &temperature, &unit) != 2 || unit != 'C')
            return false;
        display_data_update(AIR_TEMPERATURE, temperature, source);

        const char *c7 = comma(c5, 2);
        if(!c5) return false;
        if(sscanf(c7, ",%f,%c", &temperature, &unit) != 2 || unit != 'C')
            return false;
        display_data_update(WATER_TEMPERATURE, temperature, source);
    } else

    if(prefix(line, "MTA")) {
        float air_temp;
        char unit;
        if(sscanf(c1, ",%f,%c", &air_temp, &unit) != 2)
            return false;
        if(unit == 'F')
            air_temp = (air_temp-32)*.555f;
        else if(unit != 'C')
            return false;
        display_data_update(AIR_TEMPERATURE, air_temp, source);
    } else

    if(prefix(line, "RMB")) {
        float xte;
        char xte_dir;
        char from_wpt[16], to_wpt[16];
        float wpt_lat, wpt_lon;
        char wpt_lat_sign, wpt_lon_sign;
        float rng, brg, vel;
        char status;
        int ret = sscanf(c1, ",A,%f,%c,%s,%s,%f,%c,%f,%c,%f,%f,%f,%c,A,",
                         &xte, &xte_dir, from_wpt, to_wpt,
                         &wpt_lat, &wpt_lat_sign, &wpt_lon, &wpt_lon_sign, &rng, &brg, &vel, &status);
        if(ret >= 2) {
            if(xte_dir == 'L')
                xte = -xte;
            route_info.xte = xte;
        }
            
        if(ret >= 4) {
            route_info.from_wpt = from_wpt;
            route_info.to_wpt = to_wpt;
        }

        if(ret >= 8) {
            if(wpt_lat_sign == 'S')
                wpt_lat = -wpt_lat;
            if(wpt_lon_sign == 'W')
                wpt_lon = -wpt_lon;
            route_info.wpt_lat = wpt_lat;
            route_info.wpt_lon = wpt_lon;
        }
        display_data_update(ROUTE_INFO, 0, source);
    } else

    if(prefix(line, "RMC")) {
        int hour, minute;
        float second;
        float latitude, longitude;
        char status, lat_sign, lon_sign;
        float speed, track;
        uint32_t date;
        int ret1 = sscanf(c1, ",%02d%02d%f,%c", &hour, &minute, &second, &status);

        if(ret1 == 4)
            display_data_update(TIME, (hour*60+minute)*60+second, source);
        const char *c3 = comma(c1, 2);
        int ret2 = sscanf(c3, ",%f,%c,%f,%c",
                          &latitude, &lat_sign, &longitude, &lon_sign);
        if(ret2 == 4) {
            if(lat_sign == 'S')
                latitude = -latitude;
            else if(lat_sign != 'N')
                return false;
            display_data_update(LATITUDE, convert_decimal_ll(latitude), source);

            if(lon_sign == 'W')
                longitude = -longitude;
            else if(lon_sign != 'E')
                return false;
            display_data_update(LONGITUDE, convert_decimal_ll(longitude), source);
        }

        const char *c7 = comma(c3, 4);
        if(sscanf(c7, ",%f", &speed) == 1)
            display_data_update(GPS_SPEED, speed, source);

        const char *c8 = comma(c7, 1);
        if(sscanf(c8, ",%f", &track) == 1)
            display_data_update(GPS_HEADING, track, source);

        const char *c9 = comma(c8, 1);
        if(sscanf(c9, ",%lu", &date) == 1)
            // we have a time fix
            history_set_time(date, hour, minute, second);
    } else

    if(prefix(line, "VHW")) {
        const char *c5 = comma(c1, 4);
        if(!c5) return false;
        float speed;
        char unit;
        if(sscanf(c5, ",%f,%c", &speed, &unit) != 2 || unit != 'N')
            return false;
        display_data_update(WATER_SPEED, speed, source);
    } else

    if(prefix(line, "HDM")) {
        float heading;
        char unit;
        if(sscanf(c1, ",%f,%c", &heading, &unit) != 2 || unit != 'M')
            return false;

        display_data_update(COMPASS_HEADING, heading, source);
    } else

    if(prefix(line, "XDR")) {
        float value;
        const char *c4 = comma(c1, 3);
        char unit;
        if(!c4) return false;
        if(sscanf(c1, ",A,%f,%c", &value, &unit) != 2)
            return false;
        if(strncmp(c4, ",ROLL", 5) == 0)
            display_data_update(HEEL, value, source);
        else if(strncmp(c4, ",PTCH", 5) == 0)
            display_data_update(PITCH, value, source);
    } else

    if(prefix(line, "ROT")) {
        float rate;
        char unit;
        if(sscanf(c1, ",%f,%c", &rate, &unit) != 2 || unit != 'A')
            return false;

        display_data_update(RATE_OF_TURN, rate, source);
    } else

    if(prefix(line, "RSA")) {
        float angle;
        char unit;
        if(sscanf(c1, ",%f,%c", &angle, &unit) != 2 || unit != 'A')
            return false;

        display_data_update(RUDDER_ANGLE, angle, source);
    } else

    if(prefix(line, "APB")) {
        const char *c3 = comma(c1, 2);
        float xte;
        char dir;
        if(sscanf(c3, ",%f,%c,", &xte, &dir) != 2)
            return false;
        if(dir == 'L')
            xte = -xte;
        else if(dir != 'R')
            return false;

        route_info.xte = xte;

        const char *c8 = comma(c3, 5);
        float bearing_origin_destination;
        char unit;
        if(sscanf(c8, ",%f,%c,", &bearing_origin_destination, &unit) != 2)
            return false;

        const char *c10 = comma(c8, 9);
        const char *c11 = comma(c10, 1);
        char wptid[32];
        int wptlen = c11-c10-2;
        if(wptlen > sizeof wptid - 1)
            wptlen = sizeof wptid - 1;
        memcpy(wptid, c10+1, wptlen);
        wptid[wptlen] = '\0';

        float bearing_position_destination;
        char unit_pres;
        if(sscanf(c11, ",%f,%c,", &bearing_position_destination, &unit_pres) != 2 || unit_pres != 'T')
            return false;

        const char *c13 = comma(c10, 1);
        float heading_to_steer_destination;
        char unit_hdg;
        if(sscanf(c13, ",%f,%c,", &heading_to_steer_destination, &unit_hdg) != 2 || unit_hdg != 'T')
            return false;

        route_info.target_bearing = heading_to_steer_destination;


    } else
        return false;

    return true;
}

struct ClientSock
{
    ClientSock() : sock(0) {}

    void close() {
        if(!sock)
            return;
        printf("nmea close client\n");
        ::close(sock);
        sock = 0;
        data = "";
    }
    int sock;
    uint32_t time;
    std::string data;

    std::string addr;
    int port;
};

static int server_sock;
static ClientSock nmea_client;
static ClientSock pypilot_nmea_client;
static ClientSock signalk_nmea_client;
static ClientSock clients[5];

static void close_server() {
    if(!server_sock)
        return;

    printf("close nmea server %d\n", server_sock);
    for(int i=0; i<(sizeof clients)/(sizeof *clients); i++)
        clients[i].close();
    shutdown(server_sock, SHUT_RDWR);
    close(server_sock);
    server_sock = 0;
}

static void connect_client(ClientSock &client, std::string addr, int port)
{
    if(client.addr != addr || client.port != port)
        client.close();

    uint32_t t0 = millis();
    if(client.sock)
        return;

    if(t0 - client.time < 10000)
        return;

    if(addr.empty() || port == 0)
        return;

    sockaddr_in dest_addr;
    printf("try connect %d %s\n", port, addr.c_str());
    dest_addr.sin_addr.s_addr = inet_addr(addr.c_str());
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    //inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);
                
    client.sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (client.sock < 0) {
        printf("Unable to create socket: errno %d", errno);
        client.sock = 0;
        return;
    }

    // Set socket to non-blocking mode
    fcntl(client.sock, F_SETFL, fcntl(client.sock, F_GETFL, 0) | O_NONBLOCK);
                
    int err = connect(client.sock, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr_in));
    if (err != 0 && errno != EINPROGRESS) {
        printf("Socket unable to connect: errno %d\n", errno);
        client.close();
    }

    printf("nmea client connected to %s:%d %d %d\n", addr.c_str(), port, err, errno);
    client.addr = addr;
    client.port = port;
    client.time = t0;
}

static void connect_server()
{
    static int server_port;
    if(settings.nmea_server_port != server_port)
        close_server();

    if(server_sock)
        return;

    printf("nmea connect server\n");
    int addr_family = AF_INET;
    int ip_protocol = 0;

    struct sockaddr_storage dest_addr;

    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(settings.nmea_server_port);
    ip_protocol = IPPROTO_IP;

    server_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (server_sock < 0) {
        printf("unable to create server socket\n");
        server_sock = 0;
        return;
    }
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int err = bind(server_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        printf("Socket unable to bind: errno %d\n", errno);
        close_server();
        return;
    }

    err = listen(server_sock, 1);
    if (err != 0) {
        printf("Error occurred during listen: errno %d\n", errno);
        close_server();
        return;
    }
    server_port = settings.nmea_server_port;

    fcntl(server_sock, F_SETFL, fcntl(server_sock, F_GETFL, 0) | O_NONBLOCK);

    printf("nmea server setup %d\n", server_sock);
}

static bool poll_client(ClientSock &c, bool input)
{
    // read any data from client
    while(c.sock) {
        char buf[128];
        int ret = recv(c.sock, buf, sizeof buf - 1, 0);
        if(ret < 0) {
            if(errno != EAGAIN) {
                printf("client recv error %d %d\n", c.sock, errno);
                if(errno == 113) {
                    printf("ERROR 113, restart??\n");
                    //ESP.restart();
                }
                c.close();
                return false;
            } break;
        } else if(ret > 0) {
            buf[ret] = '\0';
            if(input) {
                if(c.data.length() > 4096) {
                    printf("client socket overflow!!!!!!!!\n");
                    c.data = "";
                }
                c.data += std::string(buf);
            }
        } else
            break;
        for(;;) {
            int ind = c.data.find('$', 1);
            if(ind < 0) {
                ind = c.data.find('\n');
                if(ind < 0)
                    break;
            } else
                ind --;
            std::string line = c.data.substr(0, ind);
            nmea_parse_line(line.c_str(), WIFI_DATA);
            c.data = c.data.substr(ind+1);
        }
    }
    return true;
}

static void accept_server()
{
    int keepAlive = 1;
    //int keepIdle = 1000;
    //int keepInterval = 10000;
    //int keepCount = 1;

    struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    socklen_t addr_len = sizeof(source_addr);
    int sock = accept(server_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0) {
        if(errno == EAGAIN)
            return;
        printf("Unable to accept connection: errno %d\n", errno);
        close_server();
        return;
    }

    // find empty slot
    int i = 0;
    for(;;) {
        if(clients[i].sock == 0)
            break;
        i++;
        if(i >= (sizeof clients) / (sizeof *clients)) {
            printf("too many tcp clients\n");
            close(sock);
            return;
        }
    }

    // Set tcp keepalive option
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    /*
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));*/

    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    // Convert ip address to string
    char addr_str[64];
    if (source_addr.ss_family == PF_INET)
        inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);

    printf("nmea socket accepted %d %d ip address: %s\n", i, sock, addr_str);

    clients[i].sock = sock;
    clients[i].time = millis();
}

static void poll_server()
{
    if(!server_sock)
        return;

    accept_server();
    for(int i=0; i<(sizeof clients) / (sizeof *clients); i++)
        poll_client(clients[i], settings.input_nmea_server);
}

void nmea_poll()
{
    if(!force_wifi_ap_mode &&
       settings.wifi_mode != WIFI_MODE_AP &&
       WiFi.status() != WL_CONNECTED) {
        //printf("not conn %d\n", server_sock);
        close_server();
        nmea_client.close();
        pypilot_nmea_client.close();
        signalk_nmea_client.close();
        return;
    }

    uint32_t t0 = millis();
    static uint32_t last_poll_time;

    if(t0-last_poll_time < 1000)
        return;
    last_poll_time = t0;

    if(settings.input_nmea_pypilot || settings.output_nmea_pypilot) {
        connect_client(pypilot_nmea_client, settings.pypilot_addr, 20220);
        if(!poll_client(pypilot_nmea_client, settings.input_nmea_pypilot))
            // find pypilot address again with mdns                
            pypilot_discovered=0;
    }

    if(settings.input_nmea_signalk || settings.output_nmea_signalk) {
        connect_client(signalk_nmea_client, settings.signalk_addr, 10110);
        if(!poll_client(signalk_nmea_client, settings.input_nmea_signalk))
            // find address again with mdns                
            signalk_discovered=0;
    }

    if(settings.input_nmea_client || settings.output_nmea_client) {
        connect_client(nmea_client, settings.nmea_client_addr, settings.nmea_client_port);
        poll_client(nmea_client, settings.input_nmea_client);
    }
    
    if(settings.input_nmea_server || settings.output_nmea_server) {
        connect_server();
        poll_server();
    }
}

static void write_nmea_client(ClientSock &c, const char *buf)
{
    if(!c.sock)
        return;

    int ret = send(c.sock, buf, strlen(buf), 0);
    if(ret < 0) {
        printf("errno %d\n", errno);

        if(errno == EAGAIN) {
            // timeout
            if(millis() - c.time < 10000) 
                return;
        }
    } if(ret > 0) // for now dont worry about "short" writes
        return;
        
    printf("nmea socket closed %d\n", ret);
    c.close();
}

static void write_nmea_server(const char *buf)
{
    for(int i=0; i<(sizeof clients) / (sizeof *clients); i++)
        write_nmea_client(clients[i], buf);
}

void nmea_write_serial(const char *buf, HardwareSerial *source)
{
    // handle writing to usb host serial

    if(settings.output_usb && &Serial0 != source)
        Serial0.printf(buf);
//    if(settings.rs422_1_baud_rate && &Serial1 != source)
//        Serial1.printf(buf);
    if(settings.rs422_2_baud_rate && &Serial2 != source)
        Serial2.printf(buf);
}

void nmea_write_wifi(const char *buf)
{
    if(settings.output_nmea_pypilot)
        write_nmea_client(pypilot_nmea_client, buf);
    if(settings.output_nmea_client)
        write_nmea_client(signalk_nmea_client, buf);
    if(settings.output_nmea_client)
        write_nmea_client(nmea_client, buf);
    if(settings.output_nmea_server)
        write_nmea_server(buf);
}

void nmea_send(const char *buf)
{
    char buf2[64];

    snprintf(buf2, sizeof buf2, "$QY%s*%02x\r\n", buf, 0x08^checksum(buf));
    nmea_write_serial(buf2);
    nmea_write_wifi(buf2);
}
