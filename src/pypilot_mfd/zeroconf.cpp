/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <WiFi.h>
#include "Arduino.h"

#include <ESPmDNS.h>

#include "settings.h"
#include "zeroconf.h"

static const char * if_str[] = {"STA", "AP", "ETH", "MAX"};
static const char * ip_protocol_str[] = {"V4", "V6", "MAX"};

int signalk_discovered;  // 0 means not discovered,  1 means we tried, 2 means found
int pypilot_discovered;

void mdns_analyze_results(String service_name, mdns_result_t * results)
{
    mdns_result_t * r = results;
    mdns_ip_addr_t * a = NULL;
    int i = 1, t;
    while(r) {
        
        Serial.printf("%d: Interface: %s, Type: %s\n", i++, if_str[r->tcpip_if], ip_protocol_str[r->ip_protocol]);
        if(r->instance_name){
            Serial.printf("  PTR : %s\n", r->instance_name);
        }
        if(r->hostname){
            Serial.printf("  SRV : %s.local:%u\n", r->hostname, r->port);
        }
        if(r->txt_count){
            Serial.printf("  TXT : [%u] ", r->txt_count);
            for(t=0; t<r->txt_count; t++) {
                String key = r->txt[t].key, val = r->txt[t].value;
               // Serial.printf("%s=%s; ", key.c_str(), val.c_str());
                if(service_name == "_http" && key == "swname" && val == "signalk-server") {
                    if(r->addr) {
                        char addr[64];
                        snprintf(addr, sizeof addr, IPSTR, IP2STR(&(r->addr->addr.u_addr.ip4)));
                        if(settings.signalk_addr != addr || settings.signalk_port != r->port) {
                            settings.signalk_addr = addr;
                            settings.signalk_port = r->port;
                            settings_store();
                            printf("Found signalk server %s:%d\n", addr, r->port);
                        }
                        signalk_discovered = 2;
                    }
                } else if(service_name == "_pypilot") {
                    char addr[64];
                    snprintf(addr, sizeof addr, IPSTR, IP2STR(&(r->addr->addr.u_addr.ip4)));
                    if(settings.pypilot_addr != addr) {
                        settings.pypilot_addr = addr;
                        printf("STORE\n");
                        settings_store();
                        printf("Found pypilot %s\n", addr);
                    }
                    pypilot_discovered = 2;
                }
            }

//            printf("\n");
        }
        /*
        a = r->addr;
        while(a) {
            if(a->addr.type == IPADDR_TYPE_V6)
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            else
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            a = a->next;
        }
        */
        r = r->next;
    }

}

void find_mdns_service(const char * service_name, const char * proto)
{
    Serial.printf("Query PTR: %s.%s.local\n", service_name, proto);

    mdns_result_t * results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20,  &results);
    if(err){
        Serial.println("Query Failed");
        return;
    }

    mdns_analyze_results(service_name, results);
    mdns_query_results_free(results);
}

// look for pypilot/signalk to push data to
void query_mdns_task(void *)
{
    for(;;) {
        if(WiFi.status() == WL_CONNECTED) {  
            if(pypilot_discovered <= (settings.wifi_data == NMEA_PYPILOT)) {
                pypilot_discovered = 1;
                find_mdns_service("_pypilot", "_tcp");
            }
            if(signalk_discovered <= (settings.wifi_data == SIGNALK || settings.wifi_data == NMEA_SIGNALK)) {
                signalk_discovered = 1;
                find_mdns_service("_http", "_tcp");
            }
        }
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void mdns_setup()
{
    TaskHandle_t xHandle = NULL;
    xTaskCreate(query_mdns_task,       /* Function that implements the task. */
                    "mdns",          /* Text name for the task. */
                    1024*10,      /* Stack size in words, not bytes. */
                    ( void * ) 0,    /* Parameter passed into the task. */
                    tskIDLE_PRIORITY,/* Priority at which the task is created. */
                    &xHandle );     
    if(!MDNS.begin("pypilot_mfd")) {
        Serial.println("Error starting mDNS");
        return;
    }

    MDNS.addService("http", "tcp", 80);
}
