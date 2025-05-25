#include "WifiManager.h"

#include <esp_netif.h>
#include <esp_event.h>
#include <freertos/semphr.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"

int WifiManager::init() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    printf("finished esp init");

    this->m_mutex = xSemaphoreCreateMutex();
    this->m_state = wifi_state::connect;
    this->m_attempts = 0;

    printf("finished variable init");

    printf("lambda");

    xTaskCreate(reinterpret_cast<TaskFunction_t>(s_manage), "wifi_task", 4096, this, 5, &m_task);

    printf("task create");

    return 0;
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
                printf("connect state\n");
                init_connection();
                update_state(wifi_state::connecting);
                break;
            case wifi_state::connecting:
                printf("connecting state\n");
                handle_connecting();
                break;
            case wifi_state::searching:
                printf("searching state\n");
                handle_search();
                break;
            case wifi_state::disconnect:
                printf("disconnect state\n");
                handle_disconnect();
                update_state(wifi_state::disconnected);
                break;
            default:
                printf("sleep state\n");
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
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, this);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, this);

    // TEMP: get config from nvs key value store
    wifi_config_t wifi_configuration;
    wifi_configuration = {
        .sta = {
            .ssid = "dlink-C32D",
            .password = "",
        }
    };

    esp_wifi_set_config(static_cast<wifi_interface_t>(ESP_IF_WIFI_STA), &wifi_configuration);

    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_connect();

    return 0;
}

int WifiManager::handle_connecting() {
    if (m_attempts > 10) {
        handle_disconnect();
        update_state(wifi_state::searching);
    }

    return 0;
}

int WifiManager::handle_disconnect() {
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler);

    esp_wifi_scan_stop();
    esp_wifi_disconnect();
    esp_wifi_stop();

    return 0;
}

int WifiManager::handle_search() {
    printf("Waiting for configuration");
    return 0;
}

void WifiManager::update_state(wifi_state state) {
    xSemaphoreTake(this->m_mutex, portMAX_DELAY);
    this->m_attempts = 0;
    this->m_state = state;
    xSemaphoreGive(this->m_mutex);
}

void WifiManager::wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    // Passed in as a parameter since c (freertos) cannot call the member function directly.
    const auto that = static_cast<WifiManager *>(event_handler_arg);

    if (WIFI_EVENT_STA_START == event_id) {
        printf("Connecting to wifi\n");
    } else if (WIFI_EVENT_STA_CONNECTED == event_id) {
        printf("Connected to wifi\n");
        that->update_state(wifi_state::connected);
    } else if (WIFI_EVENT_STA_DISCONNECTED == event_id) {
        printf("Disconnected from wifi\n");
        xSemaphoreTake(that->m_mutex, portMAX_DELAY);
        if (that->m_state == wifi_state::connected) {
            xSemaphoreGive(that->m_mutex);
            that->handle_disconnect();
            that->update_state(wifi_state::connect);
        } else {
            xSemaphoreGive(that->m_mutex);
        }
    } else if (IP_EVENT_STA_GOT_IP == event_id) {
        printf("Got IP \n");
    }
}
