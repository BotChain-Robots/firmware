//
// Created by Johnathon Slightham on 2025-06-12.
//

#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <variant>
#include <unordered_map>
#include <shared_mutex>

#include "enums.h"
#include "esp_check.h"
#include "nvs.h"
#include "flatbuffers_generated/RobotModule_generated.h"

// Singleton to r/w config from the ESP32 nvs (thread safe and cached)

class ConfigManager {
public:
    ConfigManager(const ConfigManager&) = delete;
    void operator=(ConfigManager &) = delete;

    static ConfigManager& get_instance() { // Thread safe as of C++11
        static ConfigManager instance;
        instance.init_config();
        return instance;
    }

    uint16_t get_module_id() const;
    ModuleType get_module_type() const;
    std::string get_wifi_ssid() const;
    std::string get_wifi_password() const;
    CommunicationMethod get_communication_method() const;

    void set_module_id(uint16_t id);
    void set_module_type(ModuleType type);
    void set_wifi_ssid(const char* ssid);
    void set_wifi_password(const char* password);
    void set_communication_method(CommunicationMethod method);

private:
    ConfigManager() = default;
    esp_err_t get_string(const char *key, std::string &out) const;
    esp_err_t get_uint16(const char *key, uint16_t *out) const;


    static esp_err_t nvs_set_cpp_str(nvs_handle_t handle, const std::string& key, const std::string& str);
    static esp_err_t nvs_get_cpp_str(nvs_handle_t handle, const std::string& key, std::string& out_str, size_t *length);

    template<typename T>
    std::optional<T> get_config_from_cache(const std::string& key) const;

    template<auto Func, typename T>
    esp_err_t set(const char* key, T value);

    template<typename T>
    bool write_to_cache(const std::string& key, T value);

    void init_config();

    std::unordered_map<std::string, std::variant<uint16_t, int8_t, std::string>> cache;
    mutable std::shared_mutex rw_lock;
    bool initialized = false;
};

#endif //CONFIGMANAGER_H
