/* Copyright (C) 2024 Sean D'Epagnier <seandepagnier@gmail.com>
 *
 * This Program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 */

#include <vector>

#include <esp_log.h>
#include <esp_littlefs.h>
#include <esp_wifi_types_generic.h>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "utils.h"
#include "settings.h"

#define TAG "settings"

using PrettyWriter = rapidjson::PrettyWriter<rapidjson::StringBuffer>;

void sensors_write_transmitters(PrettyWriter &writer, bool info);
void sensors_read_transmitters(const rapidjson::Value &t);

#define MAGIC "3A61CF00"

settings_t settings;

static std::string settings_filename = "/storage/settings.json";
bool force_wifi_ap_mode = false;
uint8_t hw_version;

void settings_t::defaults() {
#define SET_DEFAULT(type, name, def, ...) name = def;
    SETTINGS_FIELDS(SET_DEFAULT)
#undef SET_DEFAULT
}

/* read settings from json document into settings_t
   if defaults is true, replace invalid or missing values in document with default ones
   otherwise do not modify the current settings
*/

static bool read_field(std::string &x, const rapidjson::Value& v)
{
    if(v.IsString()) {
        x = v.GetString();
        return true;
    }
    
    rapidjson::StringBuffer buffer;
    PrettyWriter writer(buffer);
    //    writer.SetIndent(' ', 2);  // optional: 2 spaces (default is 4)

    v.Accept(writer);
    x = buffer.GetString();
    return true;
}

static bool read_field(bool &x, const rapidjson::Value& v)
{
    if(v.IsBool()) {
        x = v.GetBool();
        return true;
    }

    if(v.IsNumber()) {
        if(v.GetDouble() == 0)
            x = false;
        else
            x = true;
        return true;
    }
    
    if(v.IsString()) {
        std::string b = v.GetString();
        if(b == "true")
            x = true;
        else if(b == "false")
            x = false;
        else
            return false;
        return true;
    }
    return false;
}

bool read_field(float &x, const rapidjson::Value& v)
{
    if (v.IsNumber()) {
        x = v.GetDouble();
        return true;
    }

    if (v.IsString()) {
        const char *s = v.GetString();
        char* end;
        errno = 0;
        double d = std::strtod(s, &end);
        if (errno != 0 || end == s || *end != '\0')
            return false;
        x = d;
        return true;
    }
    return false;
}

static bool read_field(int &x, const rapidjson::Value& v)
{
    float y;
    if(read_field(y, v)) {
        x = y;
        return true;
    }
    return false;
}

static bool read_field(int &x, const rapidjson::Value& v, int min, int max) {
    int y;
    if(!read_field(y, v))
        return false;
    if(y < min || y > max)
        return false;
    x = y;
    return true;
}

static bool read_field(uint8_t& x, const rapidjson::Value& v) {
    int y;
    if(read_field(y, v)) {
        x = y;
        return true;
    }
    return false;
}

static bool read_field(SettingsChoice& x, const rapidjson::Value& v) {
    if(v.IsString()) {
        x.set(v.GetString());
        return true;
    }
    return false;
}
 
void settings_read(rapidjson::Document &s)
{
#define VISIT_FIELD(type, name, def, ...)        \
    if(s.HasMember(#name))                       \
        read_field(settings.name, s[#name]  __VA_OPT__(,) __VA_ARGS__);
    SETTINGS_FIELDS(VISIT_FIELD)
#undef VISIT_FIELD

    if(settings.ap_ssid.size() < 4)
        settings.ap_ssid = DEF_SSID;

    uint8_t &channel = settings.wifi_channel;
    if(channel != 1 && channel != 6 && channel != 11)
        channel = DEF_WIFI_CHANNEL;

    int &usb_baud_rate = settings.usb_baud_rate;
    if(usb_baud_rate != 38400 && usb_baud_rate != 115200)
        usb_baud_rate = 115200;

    const std::string &loglevel = settings.loglevel;
    if(loglevel == "none")
        esp_log_level_set("*", ESP_LOG_NONE);
    else if(loglevel == "error")
        esp_log_level_set("*", ESP_LOG_ERROR);
    else if(loglevel == "warn")
        esp_log_level_set("*", ESP_LOG_WARN);
    else if(loglevel == "info")
        esp_log_level_set("*", ESP_LOG_INFO);
    else if(loglevel == "debug")
        esp_log_level_set("*", ESP_LOG_DEBUG);
    else if(loglevel == "verbose")
        esp_log_level_set("*", ESP_LOG_VERBOSE);
    else
        ESP_LOGW(TAG, "invalid loglevel: %s", loglevel.c_str());

    // transmitters
    if(s.HasMember("transmitters"))
        sensors_read_transmitters(s["transmitters"]);
}

static void write_field(const std::string &x, PrettyWriter &w)
{
    w.String(x.c_str());
}

static void write_field(bool x, PrettyWriter &w)
{
    w.Bool(x);
}

static void write_field(float x, PrettyWriter &w)
{
    w.Double(x);
}

static void write_field(int x, PrettyWriter &w)
{
    w.Int(x);
}

/* return stringbuffer wtih complete */
rapidjson::StringBuffer settings_json()
{
    rapidjson::StringBuffer s;
    PrettyWriter w(s);
    w.SetMaxDecimalPlaces(6);

    w.StartObject();
    
    w.Key("magic");
    w.String(MAGIC);

#define VISIT_FIELD(type, name, def, ...)            \
        w.Key(#name);                            \
        write_field(settings.name, w);
    SETTINGS_FIELDS(VISIT_FIELD)
#undef VISIT_FIELD

    // transmitters
    w.Key("transmitters");
    sensors_write_transmitters(w, false);
    w.EndObject();

    return s;
}


////////////////////////////////////////////////////////////////////////////////////////


// read a file from disk into settings
static bool settings_load_suffix(std::string suffix="")
{
    rapidjson::Document s;

    // start with default settings
    std::string filename = settings_filename + suffix;
    bool ret = false;
    FILE *f = fopen(filename.c_str(), "rb");
    if(f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        rewind(f);

        std::vector<char> data(n+1);
    
        size_t got = fread(data.data(), n, 1, f);
        fclose(f);
        data[n] = 0;
        
        if (got != 1) return false;
        
        ESP_LOGD(TAG, "READ SETTINGS %ld %d %s", n, got, data.data());
        if(s.Parse(data.data()).HasParseError())
            ESP_LOGE(TAG, "settings file '%s' invalid/corrupted, will ignore data", filename.c_str());
        else
            ret = true;

        if(!s.IsObject() || !s.HasMember("magic") || !s["magic"].IsString() || s["magic"].GetString() != std::string(MAGIC)) {
            ret = false;
            ESP_LOGW(TAG, "settings file '%s' magic identifier failed, will ignore data", filename.c_str());
        }
    } else
        ESP_LOGW(TAG, "failed to open '%s' for reading", filename.c_str());

    if(!ret) {
        if(suffix.empty())
            return false;
        s.Parse("{}");
    }

    settings.defaults();
    settings_read(s);
    return ret;
}

static esp_err_t littlefs_mount_format_on_fail(void) {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/storage",
        .partition_label = NULL,
        .partition = NULL,
        .format_if_mount_failed = true,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = false,
    };

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    ESP_LOGW(TAG, "register returned: %s", esp_err_to_name(err));
    return err;
}

// delete settings files (they will be replaced by defaults
void settings_reset() {
    std::string filename = settings_filename;
    if(remove(filename.c_str()) != 0)
        ESP_LOGW(TAG, "failed to remove: %s", filename.c_str());
    filename += ".bak";
    if(remove(filename.c_str()) != 0)
        ESP_LOGW(TAG, "failed to remove: %s", filename.c_str());

    settings.defaults();
}

static bool settings_write(const char *cdata, int len, std::string suffix="")
{
    ESP_LOGD(TAG, "WRITE SETTINGS %s", cdata);
    std::string filename = settings_filename + suffix;
    FILE *f = fopen(filename.c_str(), "wb");
    if(!f) {
        ESP_LOGE(TAG, "failed to open '%s' for writing", settings_filename.c_str());
        return false;
    }

    fwrite((uint8_t*)cdata, len, 1, f);
    fclose(f);
    return true;
}

static bool settings_store_suffix(std::string suffix="")
{
    ESP_LOGI(TAG, "SETTINGS STORE %s", suffix.c_str());
    rapidjson::StringBuffer s = settings_json();
    return settings_write(s.GetString(), s.GetSize(), suffix);
}

void settings_store()
{
    settings_store_suffix();
}

void settings_load()
{
#ifndef __linux__
    ESP_ERROR_CHECK(littlefs_mount_format_on_fail());
#endif    
    if (settings_load_suffix())
        settings_store_suffix(".bak");
    else if(!settings_load_suffix(".bak")) {
        ESP_LOGE(TAG, "Unable to parse JSON!");
        settings_reset();
    }
    //    settings_store();

#if 0
    nvs_handle_t version_handle;
    if(nvs_open("version", NVS_READONLY, &version_handle) == ESP_OK) {
        nvs_get_u8(version_handle, "hw_version", &hw_version);
        nvs_close(version_handle);
    }

    ESP_LOGI(TAG, "Hardware Version: %d", hw_version);
    if(hw_version != 1) { // power
        gpio_hold_dis((gpio_num_t)14);
        pinMode(14, OUTPUT);  // 422 power
        digitalWrite(14, 1);  // enable 422 for now?  disable if settings disable it
    }
#endif
}

/////////////////////////////////////////////////////////////////////////
//  Accessors for serial console
/////////////////////////////////////////////////////////////////////////

static bool settings_parse(rapidjson::Document &doc) {
    rapidjson::StringBuffer s = settings_json();
    if(doc.Parse(s.GetString()).HasParseError()) {
        ESP_LOGE(TAG, "settings_keys fail to parse doc");
        return false;
    }
    return true;
}

std::unordered_set<std::string> settings_keys() {
    std::unordered_set<std::string> keys;
    rapidjson::Document s;
    if(settings_parse(s))
        for (auto& m : s.GetObject())
            keys.insert(m.name.GetString());
    return keys;
}

void settings_list() {
    rapidjson::Document s;
    if(!settings_parse(s))
        return;
    for (auto& m : s.GetObject()) {
        const char* key = m.name.GetString();
        std::string x;
        if(read_field(x, m.value))
            printf("%s %s\n", key, x.c_str());
    }
}

bool settings_get_string(const std::string &key, std::string &result) {
    rapidjson::Document s;
    if(!settings_parse(s))
        return false;

    if(!s.HasMember(key.c_str()))
        return false;

    read_field(result, s[key.c_str()]);
    return true;
}

static bool set_bool(rapidjson::Value& v, const std::string& s) {
    if (s == "true" || s == "1")
        v.SetBool(true);
    else
    if (s == "false" || s == "0")
        v.SetBool(false);
    else
        return false;
    return true;
}

static bool set_int(rapidjson::Value& v, const std::string& s, bool unsign) {
    char* end = nullptr;
    errno = 0;
    int32_t n = std::strtol(s.c_str(), &end, 0); // base 0 handles 0x.. too
    if(n < 0 && unsign)
        return false;
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    v.SetInt((int)n);
    return true;
}

static bool set_double(rapidjson::Value& v, const std::string& s) {
    char* end = nullptr;
    errno = 0;
    double d = std::strtod(s.c_str(), &end);
    if (errno != 0 || end == s.c_str() || *end != '\0') return false;
    v.SetDouble(d);
    return true;
}

// s is an object (e.g. doc["settings"])
// key exists already and you want to set it using the existing type
static bool set_by_existing_type(rapidjson::Value& v,
                                 const std::string& value,
                                 rapidjson::Document::AllocatorType& alloc)
{
    if (v.IsBool()) return set_bool(v, value);
    if (v.IsUint()) return set_int(v, value, true);
    if (v.IsInt()) return set_int(v, value, false);
    if (v.IsNumber()) return set_double(v, value);
    if (v.IsString()) {
        v.SetString(value.c_str(), (rapidjson::SizeType)value.size(), alloc);
        return true;
    }
    return false;
}

bool settings_set_value(const std::string &key, const std::string &value) {
    rapidjson::Document s;
    if(!settings_parse(s))
        return false;

    if (!s.IsObject() || !s.HasMember(key.c_str())) {
        ESP_LOGI(TAG, "settings has no key %s", key.c_str());
        return false;
    }

    if(!set_by_existing_type(s[key.c_str()], value, s.GetAllocator())) {
        ESP_LOGI(TAG, "settings_set_value failed %s", key.c_str());
        return false;
    }

    settings_read(s); // put it back in settings

    // now read back into document, validating fields
    if(!settings_parse(s))
        return false;
    
    if (!s.IsObject() || !s.HasMember(key.c_str())) {
        ESP_LOGI(TAG, "settings_set_value 2 no key %s", key.c_str());
        return false;
    }

    std::string str;
    read_field(str, s[key.c_str()]);
    ESP_LOGI(TAG, "setting %s = %s", key.c_str(), str.c_str());

    settings_store(); // write to disk
    return true;
}

bool settings_get_transmitter(const std::string &mac, const std::string &key, std::string &output, std::string *type) {
    rapidjson::StringBuffer s;
    PrettyWriter w(s);
    w.SetMaxDecimalPlaces(6);

    sensors_write_transmitters(w, true);

    rapidjson::Document doc;
    if(doc.Parse(s.GetString()).HasParseError() || !doc.IsObject()) {
        ESP_LOGE(TAG, "settings_keys fail to parse doc");
        return false;
    }

    for (auto& l : doc.GetObject()) {
        if(!l.value.IsObject()) {
            ESP_LOGE(TAG, "field in transmitters not object");
            return false;
        }

        const auto &t = l.value.GetObject();
        if(t.HasMember(mac.c_str())) {
            rapidjson::Value &w = t[mac.c_str()];
            if(key == "")
                return read_field(output, w);
            else {
                if(w.IsObject() && w.HasMember(key.c_str())) {
                    if(type)
                        *type = l.name.GetString();
                    return read_field(output, w[key.c_str()]);
                } else {
                    output = "invalid/corrupted transmitter key object";
                    return false;
                }
            }
        }
    }

    output = "mac not found";
    return false;
}
