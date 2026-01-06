#include <cstring>

#include "wireless/WifiManager.h"
#include "ConfigManager.h"
#include "constants/wifi.h"

#define TAG "WifiManager"
#define SOFTAP_SCAN_FREQUENCY_MS 30000
#define NUM_CONNECT_ATTEMPTS 5

WifiManager::~WifiManager() {
    this->handle_disconnect();
    vTaskDelete(this->m_task);
    vSemaphoreDelete(this->m_mutex);
}

int WifiManager::connect() {
    this->update_state(wifi_state::connect);
    vTaskResume(this->m_task);
    return 0;
}

int WifiManager::disconnect() {
    this->update_state(wifi_state::disconnect);
    vTaskResume(this->m_task);
    return 0;
}

[[noreturn]] void WifiManager::manage() {
    while (true) {
        xSemaphoreTake(this->m_mutex, portMAX_DELAY);
        this->m_attempts++;
        const auto state = this->m_state;
        xSemaphoreGive(this->m_mutex);

        // Wifi state machine
        switch (state) {
            case wifi_state::connect:
                ESP_LOGI(TAG, "Attempting to connect to wifi in station mode");
                init_connection();
                update_state(wifi_state::connecting);
                break;
            case wifi_state::connecting:
                ESP_LOGI(TAG, "connecting...");
                handle_connecting();
                break;
            case wifi_state::broadcast:
                ESP_LOGI(TAG, "Attempting to broadcast in softap mode");
                init_softap();
                update_state(wifi_state::broadcasting);
                break;
            case wifi_state::broadcasting:
                ESP_LOGI(TAG, "Broadcasting in softap mode");
                vTaskDelay(SOFTAP_SCAN_FREQUENCY_MS / portTICK_PERIOD_MS);  // only scan every 30 seconds, as we may disconnect users
                handle_broadcasting();                                      // scans for known networks
                break;
            case wifi_state::disconnect:
                ESP_LOGI(TAG, "Shutting down wifi");
                handle_disconnect();
                update_state(wifi_state::disconnected);
                break;
            case wifi_state::disconnected:
                ESP_LOGI(TAG, "Disconnected from wifi, starting back up");
                update_state(wifi_state::connect);
                break;
            default:
                vTaskSuspend(nullptr);
                break;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

[[noreturn]] void WifiManager::s_manage(WifiManager *that) {
    that->manage();
}

int WifiManager::init_connection() {
    if (nullptr != this->m_netif) {
        this->handle_disconnect();
    }

    this->m_netif = esp_netif_create_default_wifi_sta(); // Must be destroyed with esp_netif_destroy_default_wifi()

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, this);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, this);

    wifi_config_t wifi_configuration;
    wifi_configuration = {
        .sta = {
        }
    };

    std::string ssid = m_config_manager.get_wifi_ssid();
    std::string pass = m_config_manager.get_wifi_password();
    std::strncpy(reinterpret_cast<char *>(wifi_configuration.sta.ssid), ssid.c_str(), 32);
    std::strncpy(reinterpret_cast<char *>(wifi_configuration.sta.password), pass.c_str(), 64);
    wifi_configuration.sta.ssid[31] = '\0';
    wifi_configuration.sta.password[63] = '\0';

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(static_cast<wifi_interface_t>(ESP_IF_WIFI_STA), &wifi_configuration);

    esp_wifi_start();
    esp_wifi_connect();

    esp_netif_set_default_netif(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));

    return 0;
}

int WifiManager::handle_connecting() {
    if (this->m_attempts > NUM_CONNECT_ATTEMPTS) {
        handle_disconnect();
        update_state(wifi_state::broadcast);
    }

    return 0;
}

int WifiManager::handle_disconnect() {
    if (nullptr != this->m_netif) {
        esp_netif_destroy_default_wifi(this->m_netif);
        this->m_netif = nullptr;
    }

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler);

    esp_wifi_scan_stop();
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_clear_ap_list();
    esp_wifi_deinit();

    return 0;
}

int WifiManager::handle_broadcasting() {
    ESP_LOGI(TAG, "In softap mode, scanning for known networks");
    wifi_scan_config_t scan_config {};
    scan_config.ssid = reinterpret_cast<uint8_t *>(m_config_manager.get_wifi_ssid().data());
    scan_config.scan_time = {.passive = 500};

    if (const auto err = esp_wifi_scan_start(&scan_config, false); ESP_OK != err) {
        ESP_LOGE(TAG, "Failed to scan for wifi networks, err: %d", err);
        esp_wifi_clear_ap_list(); // must call to free memory allocated by scan.
        return -1;
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    uint16_t found_aps = 0;
    if (const auto err = esp_wifi_scan_get_ap_num(&found_aps); ESP_OK != err) {
        ESP_LOGE(TAG, "Failed to get count of scanned aps");
        esp_wifi_clear_ap_list();
        return -1;
    }

    if (found_aps > 1) {
        ESP_LOGI(TAG, "Found a known network, switching to station mode");
        update_state(wifi_state::disconnect);
    }

    esp_wifi_clear_ap_list(); // must call to free memory allocated by scan.
    return 0;
}

int WifiManager::init_softap() {
    if (nullptr != this->m_netif) {
        this->handle_disconnect();
    }

    this->m_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, this);

    wifi_config_t wifi_configuration = {
        .ap = {
            .ssid = WIFI_SSID,
            .password = "",
            .channel = SOFTAP_CHANNEL,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            .max_connection = SOFTAP_MAX_CONNECTIONS,
            .beacon_interval = 100,
            .csa_count = 3,
            .dtim_period = 1,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_APSTA); // enable both ap and station mode for scanning
    esp_wifi_set_config(static_cast<wifi_interface_t>(ESP_IF_WIFI_AP), &wifi_configuration);

    esp_wifi_start();

   	esp_netif_set_default_netif(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));

    return 0;
}

void WifiManager::update_state(const wifi_state state) {
    xSemaphoreTake(this->m_mutex, portMAX_DELAY);
    this->m_attempts = 0;
    this->m_state = state;
    xSemaphoreGive(this->m_mutex);
}

void WifiManager::wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, const int32_t event_id, void *event_data) {
    // Passed in as a parameter since c (freertos) cannot call the member function directly.
    const auto that = static_cast<WifiManager *>(event_handler_arg);

    if (WIFI_EVENT_STA_START == event_id) {
        ESP_LOGI(TAG, "Station mode started");
    } else if (WIFI_EVENT_STA_CONNECTED == event_id) {
        ESP_LOGI(TAG, "Connected to wifi in station mode");
        that->update_state(wifi_state::connected);
    } else if (WIFI_EVENT_STA_DISCONNECTED == event_id) {
        ESP_LOGI(TAG, "Station mode shutdown");
        xSemaphoreTake(that->m_mutex, portMAX_DELAY);
        if (that->m_state == wifi_state::connected) {
            xSemaphoreGive(that->m_mutex);
            that->handle_disconnect();
        } else {
            xSemaphoreGive(that->m_mutex);
        }
    } else if (IP_EVENT_STA_GOT_IP == event_id) {
        ESP_LOGI(TAG, "Got IP as station");
    } else if (WIFI_EVENT_AP_STACONNECTED == event_id) {
        ESP_LOGI(TAG, "User connected to AP");
    } else if (WIFI_EVENT_AP_STADISCONNECTED == event_id) {
        ESP_LOGI(TAG, "User disconnected from AP");
    } else if (WIFI_EVENT_AP_START == event_id) {
        ESP_LOGI(TAG, "AP started");
    }
}
