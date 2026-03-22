/* Copyright (C) 2026 Sean D'Epagnier <seandepagnier@proton.me>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <math.h>
#include <string.h>

#include <string>
#include <sstream>
#include <unordered_set>

#include <esp_timer.h>
#include <driver/uart.h>

#include "linenoise.h"

#include "settings.h"
#include "sensors.h"
#include "wireless.h"
#include "serial.h"
#include "nmea.h"
#include "web.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

// This macro is what FreeRTOS uses for runtime stats
#ifndef portGET_RUN_TIME_COUNTER_VALUE
#warning "portGET_RUN_TIME_COUNTER_VALUE not defined"
#endif

#define TAG "serial"

using arg_list = const std::vector<std::string>;

static void nmea_write_serial_internal(const char *buf, int uart=-1);

static void cpu_usage()
{
    UBaseType_t n = uxTaskGetNumberOfTasks();
    TaskStatus_t st[n];

    uint32_t dt_total;
    UBaseType_t got = uxTaskGetSystemState(st, n, &dt_total);
    if (dt_total == 0) dt_total = 1; // avoid divide-by-zero if timer resolution is coarse

    printf("%-16s %10s %8s %8s\n", "Task", "Ticks", "Percent","CPU Core");
    // percent = task_delta / total_delta * 100
    for (UBaseType_t i = 0; i < got; i++) {
        uint32_t dt = st[i].ulRunTimeCounter;
        uint32_t pct_x10 = (uint32_t)((uint64_t)dt * 1000ULL / (uint64_t)dt_total); // 0.1% units
        printf("%-16s %10" PRIu32 " %6" PRIu32 ".%01" PRIu32 "%% ",
               st[i].pcTaskName, dt, pct_x10 / 10, pct_x10 % 10);
        if(st[i].xCoreID > 1)
            printf("0/1\n");
        else
            printf("%d\n", st[i].xCoreID);
    }
}

typedef struct {
    const char *name;
    uint32_t caps;   // heap capability flags
} mem_region_t;

static void print_region(const char *name, uint32_t caps)
{
    multi_heap_info_t info;
    heap_caps_get_info(&info, caps);

    // info.total_free_bytes is free; info.total_allocated_bytes is allocated
    size_t total = info.total_free_bytes + info.total_allocated_bytes;
    size_t freeb = info.total_free_bytes;
    size_t used  = info.total_allocated_bytes;

    double used_pct = (total > 0) ? (100.0 * (double)used / (double)total) : 0.0;

    printf("%-14s total=%7u  used=%7u  free=%7u  used=%5.1f%%  largest_free=%7u\n",
           name,
           (unsigned)total,
           (unsigned)used,
           (unsigned)freeb,
           used_pct,
           (unsigned)info.largest_free_block);
}

void meminfo_print_heap(void)
{
    // Common regions/capabilities. Depending on chip/config, some may be 0.
    const mem_region_t regions[] = {
        { "INTERNAL",  MALLOC_CAP_INTERNAL },
        { "8BIT",      MALLOC_CAP_8BIT     },
        { "DMA",       MALLOC_CAP_DMA      },
        { "SPIRAM",    MALLOC_CAP_SPIRAM   },
        // You can add others like MALLOC_CAP_EXEC, MALLOC_CAP_DEFAULT, etc.
    };

    printf("---- Heap by region ----\n");
    for (size_t i = 0; i < sizeof(regions)/sizeof(regions[0]); i++)
        print_region(regions[i].name, regions[i].caps);

    // Overall (all heaps combined)
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free  = esp_get_minimum_free_heap_size(); // "minimum ever" free heap
    printf("---- Overall ----\n");
    printf("free_heap=%u  min_ever_free_heap=%u\n",
             (unsigned)free_heap, (unsigned)min_free);
}

/*
 * Optional: print stack HWM for all tasks.
 * Note: uxTaskGetSystemState allocates; use with care on very low memory systems.
 */
void meminfo_print_all_task_stack_hwm(void)
{
    UBaseType_t n = uxTaskGetNumberOfTasks();
    TaskStatus_t st[n];

    uint32_t total_run_time = 0;
    UBaseType_t got = uxTaskGetSystemState(st, n, &total_run_time);

    printf("---- Stack HWM (all tasks) ---- runtime: %lu\n", total_run_time);
    for (UBaseType_t i = 0; i < got; i++) {
        size_t free_bytes = (size_t)st[i].usStackHighWaterMark * sizeof(StackType_t);
        printf("%-16s min_ever_free=%6u bytes  prio=%u\n",
               st[i].pcTaskName,
               (unsigned)free_bytes,
               (unsigned)st[i].uxCurrentPriority);
    }
}

static void mem()
{
    meminfo_print_heap();
    meminfo_print_all_task_stack_hwm(); // all tasks (optional)
}

static bool is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static bool is_valid_mac(const std::string& mac)
{
    if (mac.size() != 17)
        return false;

    for (int i = 0; i < 17; i++) {
        if ((i % 3) == 2) {
            if (mac[i] != ':')
                return false;
        } else {
            if (!is_hex_digit(mac[i]))
                return false;
        }
    }
    return true;
}

static std::vector<std::string> split_ws(const std::string& s)
{
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok)
        out.push_back(tok);
    return out;
}

static bool get_exec(arg_list &args) {
    std::string value;
    if(!settings_get_string(args[1], value)) {
        printf("failed to get %s\n", args[1].c_str());
        return false;
    }
    printf("%s\n", value.c_str());
    return true;
}

static void get_completion(const arg_list& args, linenoiseCompletions* lc)
{
    if(args.size() > 2)
        return;

    std::unordered_set<std::string> keys = settings_keys();
    for(auto &key : keys)
        if(args.size() == 1 || key.starts_with(args[1]))
            linenoiseAddCompletion(lc, (args[0] + " " + key).c_str());
}

static bool set_exec(arg_list &args) {
    std::string name  = args[1];
    if(name == "transmitters") {
        printf("do not use set on transmitters, use transmitter command\n");
        return false;
    }
    
    std::string value = args[2];

    printf("set '%s' = '%s'\n", name.c_str(), value.c_str());
    std::unordered_set<std::string> keys = settings_keys();
    if(keys.find(name) == keys.end()) {
        ESP_LOGE(TAG, "invalid key: %s\n", name.c_str());
        printf("try 'list'\n");
        return false;
    }

    if(!settings_set_value(name, value)) {
        printf("failed to set '%s'='%s'\n", name.c_str(), value.c_str());
        return false;
    }

    return true;
}

static bool transmitters_exec(arg_list &args) {
    switch(args.size()) {
    case 1: {
        std::string value;
        if(!settings_get_string("transmitters", value))
            return false;
        printf("%s\n", value.c_str());
    } return true;
    case 2:
        if (args[1] == "help") {
            printf("\nUsage: transmitters <mac> [ap, key, key=value]\n");
            printf("list all transmitters (alias for get transmitters)\n");
            printf(">> transmitters\n");
            printf("list specific transmitter by mac address\n");
            printf(">> transmitters d0:ef:76:68:b1:b8\n");
            printf("enable access point (connect directly via browser)\n");
            printf(">> transmitters d0:ef:76:68:b1:b8 ap\n");
            printf("get a setting\n");
            printf(">> transmitters d0:ef:76:68:b1:b8 position\n");
            printf("modify settings\n");
            printf(">> transmitters d0:ef:76:68:b1:b8 position primary\n\n");
        } else {
            if (!is_valid_mac(args[1])) {
                printf("Invalid MAC address: %s\n", args[1].c_str());
                printf("Try: transmitters help\n");
            } else {
                // transmitters <mac>
                std::string out;
                settings_get_transmitter(args[1], "", out);
                printf("%s\n", out.c_str());
                return true;
            }
        } break;
    case 3:
        // transmitters <mac> <action>
        if (args[2] == "ap")
            return wireless_unlock_channel(args[1]);
        {
            std::string out;
            bool ret = settings_get_transmitter(args[1], args[2], out);
            printf("%s\n", out.c_str());
            return ret;
        }
    case 4: {
        std::string mac = args[1], key = args[2], value = args[3], out, type;
        if(settings_get_transmitter(mac, key, out, &type)) {
            char buf[256];
            snprintf(buf, sizeof buf, "{\"%s\":{\"%s\":{\"%s\":\"%s\"}}}",
                     type.c_str(), mac.c_str(), key.c_str(), value.c_str());
            
            rapidjson::Document doc;
            if(doc.Parse(buf).HasParseError()) {
                ESP_LOGE(TAG, "settings_keys fail to parse doc");
            } else {
                void sensors_read_transmitters(const rapidjson::Value &t);
                sensors_read_transmitters(doc);
                settings_store();
                return true;
            }
        } else
            printf("invalid key, try 'transmitters %s'\n", args[1].c_str());
    } break;
    default:
        printf("Too many arguments\n");
        printf("Try: transmitters help\n");
    }
    return false;
}

static void transmitters_completion(const arg_list& args, linenoiseCompletions* lc)
{
    std::unordered_set<std::string> macs = sensors_macs();

    for(auto &mac : macs) {
        if(args.size() >= 2 && mac == args[1]) {
            std::string beg = args[0] + " " + mac + " ";
            linenoiseAddCompletion(lc, (beg + "ap").c_str());
            linenoiseAddCompletion(lc, (beg + "position").c_str());
        }
                
        if(args.size() == 1 ||
           (args.size() == 2 && mac.starts_with(args[1])))
            linenoiseAddCompletion(lc, (args[0] + " " + mac).c_str());
    }
}

struct command {
    const char* cmd;
    const char* help;
    void (*exec_simple)();
    bool (*exec)(const arg_list& args);
    void (*completion)(const arg_list& args, linenoiseCompletions* lc);
};

static void help();
static const command commands[] = {
    {"cpu",    "print cpu info",               cpu_usage,         NULL, NULL},
#ifdef CONFIG_IDF_TARGET_ESP32S3
    {"display_auto", "automatically enable relevant display pages",
     [](const arg_list&) { display_auto(); return true; }, NULL},
#endif
    {"get", "show value of setting",           NULL, get_exec, get_completion},
    {"help",   "print this help message",      help,              NULL, NULL},
    {"list", "list settings",                  settings_list,     NULL, NULL},
    {"mem",    "print memory info",            mem,               NULL, NULL},
    {"reboot", "reboot device",                abort,             NULL, NULL},
    {"scan_wifi", "scan wireless networks",    wireless_scan,     NULL, NULL},
    {"set", "set a setting",                   NULL, set_exec, get_completion},
    {"settings_reset", "reset settings",       settings_reset,    NULL, NULL},
    {"transmitters", "configure transmitters", NULL, transmitters_exec,
    transmitters_completion},
};

static void help() {
    printf("Commands:\n");
    for (const auto& command : commands)
        printf("%s -- %s\n", command.cmd, command.help);
}

bool serial_process_line(const std::string buf) {
    auto args = split_ws(buf);
    if(args.size() == 0)
        return false;

    for (const auto& command : commands)
        if(command.cmd == args[0]) {
            if(command.exec_simple) {
                command.exec_simple();
                return true;
            } else
                return command.exec(args);
        }
    printf("command not found, try 'help'\n");
    return false;
}

static void linenoise_completion(const char *buf, linenoiseCompletions *lc)
{
    auto args = split_ws(buf);
    if(args.size() == 0) {
        for (const auto& command : commands)
            linenoiseAddCompletion(lc, command.cmd);
        return;
    }

    for (const auto& command : commands) {
        if(command.cmd == args[0] &&
           command.completion)
            command.completion(args, lc);

        if(args.size() == 1 &&
           std::string(command.cmd).starts_with(args[0]))
            linenoiseAddCompletion(lc, command.cmd);
    }
}

static ssize_t linenoise_read(int fd, void* inb, size_t len) {
    return uart_read_bytes(UART_NUM_0, inb, len, 0); // 0 = non-blocking
}

struct SerialLinebuffer {
    SerialLinebuffer(uart_port_t u, data_source_e so)
        : uart(u), source(so), lastcli_t0(0), linenoiseinit(false), linenoiserestart(true) {}

    std::string readline() {
        char inb[256];
        for (;;) {
            int len = uart_read_bytes(UART_NUM_0, inb, sizeof(inb)-1, 0); // 0 = non-blocking
            if (len <= 0)
                return "";
            int start = buf.length();
            inb[len] = 0;
            buf += inb;
            if((buf.length() == 0 && inb[0] != '$') || buf[0] != '$')
                uart_write_bytes(UART_NUM_0, inb, len); // echo if not nmea

            for(int i=start; i<buf.length(); i++) {
                char c = buf[i];
                if (c == '\n' || c == '\r') {
                    std::string data = buf.substr(0, i+1);
                    buf = buf.substr(i+1);
                    if (data.empty())
                        continue;
                    return data;
                }
                if (c == '\b' || c == 127) {
                    if(i > 0)
                        buf.erase(i-1, 2);
                    else
                        buf.erase(i, 1);
                    i--;
                    continue;
                }
            }
            if(buf.length() > 256)
                buf = "";
        }
    }

    std::string linenoiseread() {
        char *line = linenoiseEditMore;
        while(line == linenoiseEditMore)
            line = linenoiseEditFeed(&LineNoiseState);
        if (!line) return "";
        linenoiseEditStop(&LineNoiseState);
        linenoiserestart = true;

        std::string ret = line;
        linenoiseFree(line);
        return ret;
    }

    void read(bool enabled = true, bool usecli = false) {
        for (;;) {
            if(!linenoiseinit) {
                linenoiseSetCompletionCallback(linenoise_completion);
                linenoiseSetReadFunction(linenoise_read);
                linenoiseHistorySetMaxLen(20);
                linenoiseinit = true;
            }

            if(linenoiserestart) {
                linenoiseEditStart(&LineNoiseState,-1,-1,LineNoiseBuffer,sizeof(LineNoiseBuffer),"> ");
                linenoiserestart = false;
            }        
            
            std::string line = linenoiseread();

            if (line.empty())
                break;

            if(line[0] != '$') {
                if(usecli) {
                    lastcli_t0 = esp_timer_get_time();
                    if(serial_process_line(line))
                        linenoiseHistoryAdd(line.c_str());
                }
                continue;
            }

#ifdef CONFIG_IDF_TARGET_ESP32S3
            if (enabled)
                nmea_parse_line(line.c_str(), source);
#endif

            line += "\r\n";
#ifdef CONFIG_IDF_TARGET_ESP32S3
            if(settings.forward_nmea_serial_to_serial)
                nmea_write_serial_internal(line.c_str(), uart);
#endif
            if(settings.forward_nmea_serial_to_wifi)
                nmea_write_wifi(line.c_str());
        }
    }

    void write(const char *buf) {
        if(lastcli_t0 != 0) {
            // prevent serial output within 20 seconds of commandline interface command
            uint64_t t0 = esp_timer_get_time();
            if(t0 - lastcli_t0 < 20e6)
                return;
        }
        uart_write_bytes(uart, buf, strlen(buf));
    }

    std::string buf;
    uart_port_t uart;
    data_source_e source;
    uint64_t lastcli_t0;

    struct linenoiseState LineNoiseState;
    char LineNoiseBuffer[256];
    bool linenoiseinit, linenoiserestart;
};

SerialLinebuffer Serial0Buffer(UART_NUM_0, USB_DATA);
#ifdef CONFIG_IDF_TARGET_ESP32S3
SerialLinebuffer Serial1Buffer(Serial1, RS422_DATA);
SerialLinebuffer Serial2Buffer(Serial2, RS422_DATA);
#endif

void serial_setup()
{
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0));

    const uart_port_t uart_num = UART_NUM_0;
    int baud = settings.usb_baud_rate;
    if(baud != 38400)
        baud = 115200;

    uart_config_t uart_config = {};
    uart_config.baud_rate = baud;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    
#ifdef CONFIG_IDF_TARGET_ESP32S3
    //    Serial0.begin(settings.usb_baud_rate);
    //    Serial0.setTimeout(0);
    bool any;

    // usb host serial here
    Serial0.end();
    if(settings.input_usb || settings.output_usb) {
        Serial0.begin(settings.usb_baud_rate);
        Serial0.setTimeout(0);
        any = true;
        extio_set(EXTIO_ENA_NMEA0);
    } else
        extio_clr(EXTIO_ENA_NMEA0);

    // TODO: enable power for 422 if either serial is enabled
    /*
    Serial1.end();
    if(settings.rs422_1_baud_rate) {
        Serial1.begin(settings.rs422_1_baud_rate,
                      SERIAL_8N1, 15, 2);  //Hardware Serial of ESP32
        Serial1.setTimeout(0);
        extio_set(EXTIO_ENA_NMEA1);
    } else
        extio_clr(EXTIO_ENA_NMEA1);
    */

    Serial2.end();
    if(settings.rs422_2_baud_rate) {
        extio_set(EXTIO_ENA_NMEA2);
        Serial2.begin(settings.rs422_2_baud_rate,
                      SERIAL_8N1, 16, 17);  //Hardware Serial of ESP32
        Serial2.setTimeout(0);
        any = true;
    } else
        extio_clr(EXTIO_ENA_NMEA2);

    extio_set(EXTIO_ENA_NMEA, any);
#endif
}

void serial_poll() {

    // read usb host serial
    Serial0Buffer.read(settings.input_usb, true);
    //Serial1Buffer.read(settings.rs422_1_baud_rate);
    //    Serial2Buffer.read(settings.rs422_2_baud_rate);
}

static void nmea_write_serial_internal(const char *buf, int uart)
{
    // handle writing to usb host serial
    if(settings.output_usb && UART_NUM_0 != uart)
        Serial0Buffer.write(buf);

//    if(settings.rs422_1_baud_rate && &Serial1 != source)
//        Serial1.printf(buf);
//    if(settings.rs422_2_baud_rate && &Serial2 != source)
//        Serial2.printf(buf);
}

void serial_write_nmea(const char *buf) {
    nmea_write_serial_internal(buf);
}
