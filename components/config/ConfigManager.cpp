//
// Created by Johnathon Slightham on 2025-06-12.
//

#include "nvs_flash.h"
#include "constants/config.h"

#include "ConfigManager.h"

void ConfigManager::init_config() {
    nvs_flash_init();

    nvs_handle config_handle;
    nvs_open(NVS_FLASH_NAMESPACE, NVS_READWRITE, &config_handle);

    uint16_t id;
    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u16(config_handle, MODULE_ID_KEY, &id)) {
        nvs_set_u16(config_handle, MODULE_ID_KEY, DEFAULT_MODULE_ID);
    }

    uint16_t type;
    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_u16(config_handle, MODULE_TYPE_KEY, &type)) {
        nvs_set_u16(config_handle, MODULE_TYPE_KEY, DEFAULT_MODULE_TYPE);
    }

    nvs_close(config_handle);
}

// todo: we should probably cache some of these things
uint16_t ConfigManager::get_module_id() {
    nvs_handle config_handle;
    nvs_open(NVS_FLASH_NAMESPACE, NVS_READONLY, &config_handle);
    uint16_t id;
    nvs_get_u16(config_handle, MODULE_ID_KEY, &id);
    nvs_close(config_handle);
    return id;
}

ModuleType ConfigManager::get_module_type() {
    nvs_handle config_handle;
    nvs_open(NVS_FLASH_NAMESPACE, NVS_READONLY, &config_handle);
    uint16_t type;
    nvs_get_u16(config_handle, MODULE_TYPE_KEY, &type);
    nvs_close(config_handle);
    return static_cast<ModuleType>(type);
}

std::string ConfigManager::get_wifi_ssid() {
    return get_string(WIFI_SSID_KEY);
}

std::string ConfigManager::get_wifi_password() {
    return get_string(WIFI_PASSWORD_KEY);
}

std::string ConfigManager::get_string(const char *key) {
    nvs_handle config_handle;
    nvs_open(NVS_FLASH_NAMESPACE, NVS_READONLY, &config_handle);

    size_t required_size; // get size of the string
    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_str(config_handle, key, nullptr, &required_size)) {
        return "";
    }

    std::string str;
    str.resize(required_size);
    if (ESP_ERR_NVS_NOT_FOUND == nvs_get_str(config_handle, key, str.data(), &required_size)) {
        return "";
    }

    nvs_close(config_handle);
    return str;
}

void ConfigManager::set_module_id(const uint16_t id) {
    nvs_handle config_handle;
    nvs_open(NVS_FLASH_NAMESPACE, NVS_READWRITE, &config_handle);
    nvs_set_u16(config_handle, MODULE_ID_KEY, id);
    nvs_close(config_handle);
}

void ConfigManager::set_module_type(const ModuleType type) {
    nvs_handle config_handle;
    nvs_open(NVS_FLASH_NAMESPACE, NVS_READWRITE, &config_handle);
    nvs_set_u16(config_handle, MODULE_TYPE_KEY, type);
    nvs_close(config_handle);
}

void ConfigManager::set_wifi_ssid(const char* ssid) {
    nvs_handle config_handle;
    nvs_open(NVS_FLASH_NAMESPACE, NVS_READWRITE, &config_handle);
    nvs_set_str(config_handle, WIFI_SSID_KEY, ssid);
    nvs_close(config_handle);
}

void ConfigManager::set_wifi_password(const char* password) {
    nvs_handle config_handle;
    nvs_open(NVS_FLASH_NAMESPACE, NVS_READWRITE, &config_handle);
    nvs_set_str(config_handle, WIFI_PASSWORD_KEY, password);
    nvs_close(config_handle);
}

