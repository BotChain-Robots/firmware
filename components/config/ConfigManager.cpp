//
// Created by Johnathon Slightham on 2025-06-12.
//


#include <mutex>
#include <esp_log.h>

#include "nvs_flash.h"
#include "constants/config.h"
#include "ConfigManager.h"

static auto TAG = "ConfigManager";

void ConfigManager::init_config() {
    std::unique_lock lock(rw_lock);
    if (this->initialized)
        return;
    this->initialized = true;
    lock.unlock();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased, retry nvs_flash_init after erasing
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

uint16_t ConfigManager::get_module_id() const {
    uint16_t i = DEFAULT_MODULE_ID;
    get_uint16(MODULE_ID_KEY, &i);
    return i;
}

ModuleType ConfigManager::get_module_type() const {
    uint16_t type = DEFAULT_MODULE_TYPE;
    get_uint16(MODULE_TYPE_KEY, &type);
    return static_cast<ModuleType>(type);
}

std::string ConfigManager::get_wifi_ssid() const {
    std::string str;
    get_string(WIFI_SSID_KEY, str);
    return str;
}

std::string ConfigManager::get_wifi_password() const {
    std::string str;
    get_string(WIFI_PASSWORD_KEY, str);
    return str;
}

esp_err_t ConfigManager::get_string(const char *key, std::string& out) const {
    std::shared_lock lock(rw_lock);
    esp_err_t ret = ESP_OK;
    if (const auto str = get_config_from_cache<std::string>(key)) {
        out = *str;
        return ret;
    }

    ESP_LOGD(TAG, "get_string cache miss");

    nvs_handle config_handle;
    if (ret = nvs_open(NVS_FLASH_NAMESPACE, NVS_READONLY, &config_handle); ret != ESP_OK) {
        ESP_LOGE(TAG, "get_string Failed to open NVS");
        return ret;
    }

    size_t required_size; // get size of the string
    if (ret = nvs_get_str(config_handle, key, nullptr, &required_size); ret != ESP_OK) {
        ESP_LOGE(TAG, "get_string failed to get string size");
        return ret;
    }

    out.resize(required_size);
    if (ret = nvs_get_str(config_handle, key, out.data(), &required_size); ret != ESP_OK) {
        ESP_LOGE(TAG, "get_string failed to get string");
        return ret;
    }

    nvs_close(config_handle);
    return ret;
}

esp_err_t ConfigManager::get_uint16(const char *key, uint16_t *out) const {
    esp_err_t ret = ESP_OK;
    std::shared_lock lock(rw_lock);
    if (const auto i = get_config_from_cache<uint16_t>(key)) {
        *out = i.value();
        return ret;
    }

    ESP_LOGD(TAG, "get_uint16 cache miss");

    nvs_handle config_handle;
    if (ret = nvs_open(NVS_FLASH_NAMESPACE, NVS_READONLY, &config_handle); ret != ESP_OK) {
        return ret;
    }

    if (ret = nvs_get_u16(config_handle, key, out); ret != ESP_OK) {
        return ret;
    }

    nvs_close(config_handle);
    return ret;
}

template<typename T>
std::optional<T> ConfigManager::get_config_from_cache(const std::string& key) const {
    // Not thread safe
    if (const auto it = cache.find(key); it != cache.end()) {
        if (auto val = std::get_if<T>(&it->second))
            return *val;
    }
    return std::nullopt;
}

void ConfigManager::set_module_id(const uint16_t id) {
    set<nvs_set_u16, uint16_t>(MODULE_ID_KEY, id);
}

void ConfigManager::set_module_type(const ModuleType type) {
    set<nvs_set_u16, uint16_t>(MODULE_TYPE_KEY, type);
}

void ConfigManager::set_wifi_ssid(const char* ssid) {
    set<nvs_set_cpp_str, std::string>(WIFI_SSID_KEY, ssid);
}

void ConfigManager::set_wifi_password(const char* password) {
    set<nvs_set_cpp_str, std::string>(WIFI_PASSWORD_KEY, password);
}

// Func - the esp write function to call (ie. nvs_set_u16)
// T - the type of the value in the key value pair
template<auto Func, typename T>
esp_err_t ConfigManager::set(const char* key, const T value) {
    std::unique_lock lock(rw_lock);

    if (write_to_cache(key, value)) {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    nvs_handle config_handle;
    ret = nvs_open(NVS_FLASH_NAMESPACE, NVS_READWRITE, &config_handle);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "set error opening NVS handle");
        return ret;
    }

    ret = Func(config_handle, key, value);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "set error writing to NVS key %s", key);
    }
    nvs_close(config_handle);

    return ret;
}

// Returns true when we don't need to write through the cache
template<typename T>
bool ConfigManager::write_to_cache(const std::string& key, const T value) {
    const auto it = cache.find(key);
    if (it != cache.end()) {
        if (const auto existing = std::get_if<T>(&it->second)) {
            if (*existing == value)
                return true;
        }
    }
    cache[key] = value;
    return false;
}

esp_err_t ConfigManager::nvs_set_cpp_str(const nvs_handle_t handle, const std::string& key, const std::string& str) {
    return nvs_set_str(handle, key.c_str(), str.c_str());
}

esp_err_t ConfigManager::nvs_get_cpp_str(const nvs_handle_t handle, const std::string& key, std::string& out_str, size_t *length) {
    return nvs_get_str(handle, key.c_str(), out_str.data(), length);
}
