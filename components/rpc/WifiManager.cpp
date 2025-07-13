#include "WifiManager.h"

#include <esp_netif.h>
#include <esp_event.h>
#include <freertos/semphr.h>
#include <cstring>
#include <mDNSDiscoveryService.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "constants/wifi.h"

WifiManager::WifiManager() {
    esp_netif_init();
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_event_loop_create_default();

    this->m_mutex = xSemaphoreCreateMutex();
    this->m_state = wifi_state::disconnected;
    this->m_attempts = 0;
    this->m_task = nullptr;
    this->m_netif = nullptr;

    xTaskCreate(reinterpret_cast<TaskFunction_t>(s_manage), "wifi_task", 3096, this, 5, &m_task);
}

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

        switch (state) {
            case wifi_state::connect:
                printf("Attempting to connect to wifi in station mode\n");
                init_connection();
                update_state(wifi_state::connecting);
                break;
            case wifi_state::connecting:
                printf("connecting...\n");
                handle_connecting();
                break;
            case wifi_state::broadcast:
                printf("Attempting to broadcast in softap mode\n");
                init_softap();
                update_state(wifi_state::broadcasting);
                break;
            case wifi_state::disconnect:
                printf("Shutting down wifi\n");
                handle_disconnect();
                update_state(wifi_state::disconnected);
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

    // TEMP: get config from nvs key value store
    wifi_config_t wifi_configuration;
    wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = "",
        }
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(static_cast<wifi_interface_t>(ESP_IF_WIFI_STA), &wifi_configuration);

    esp_wifi_start();
    esp_wifi_connect();

    return 0;
}

int WifiManager::handle_connecting() {
    if (this->m_attempts > 10) {
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

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(static_cast<wifi_interface_t>(ESP_IF_WIFI_AP), &wifi_configuration);

    esp_wifi_start();

    return 0;
}

void WifiManager::update_state(wifi_state state) {
    xSemaphoreTake(this->m_mutex, portMAX_DELAY);
    this->m_attempts = 0;
    this->m_state = state;
    xSemaphoreGive(this->m_mutex);
}

void WifiManager::wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, const int32_t event_id, void *event_data) {
    // Passed in as a parameter since c (freertos) cannot call the member function directly.
    const auto that = static_cast<WifiManager *>(event_handler_arg);

    if (WIFI_EVENT_STA_START == event_id) {
        printf("Station mode started\n");
    } else if (WIFI_EVENT_STA_CONNECTED == event_id) {
        printf("Connected to wifi in station mode\n");
        that->update_state(wifi_state::connected);
        mDNSDiscoveryService::setup();
    } else if (WIFI_EVENT_STA_DISCONNECTED == event_id) {
        printf("Station mode shutdown\n");
        xSemaphoreTake(that->m_mutex, portMAX_DELAY);
        if (that->m_state == wifi_state::connected) {
            xSemaphoreGive(that->m_mutex);
            that->handle_disconnect();
            that->update_state(wifi_state::connect);
        } else {
            xSemaphoreGive(that->m_mutex);
        }
    } else if (IP_EVENT_STA_GOT_IP == event_id) {
        printf("Got IP as station\n");
    } else if (WIFI_EVENT_AP_STACONNECTED == event_id) {
        printf("User connected to AP\n");
    } else if (WIFI_EVENT_AP_STADISCONNECTED == event_id) {
        printf("User disconnected from AP\n");
    } else if (WIFI_EVENT_AP_START == event_id) {
        mDNSDiscoveryService::setup();
        printf("AP started\n");
    }
}
