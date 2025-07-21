//
// Created by Johnathon Slightham on 2025-06-12.
//

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include "constants/config.h"
#include "flatbuffers_generated/RobotModule_generated.h"

class ConfigManager {
public:
    static void init_config();

    static uint16_t get_module_id();
    static ModuleType get_module_type();
    static std::string get_wifi_ssid();
    static std::string get_wifi_password();

    static void set_module_id(uint16_t id);
    static void set_module_type(ModuleType type);
    static void set_wifi_ssid(const char* ssid);
    static void set_wifi_password(const char* password);

private:
    static std::string get_string(const char* key);

};

#endif //CONFIGMANAGER_H
