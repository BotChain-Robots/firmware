#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "esp_netif_types.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "ConfigManager.h"
#include "IWifiManager.h"

class WifiManager final : IWifiManager {
public:
  WifiManager() : m_config_manager(ConfigManager::get_instance()),
                  m_mutex(xSemaphoreCreateMutex()),
                  m_state(wifi_state::disconnected) {
    esp_netif_init();
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_event_loop_create_default();
    // todo: move all task metadata to a constants/config file
    xTaskCreate(reinterpret_cast<TaskFunction_t>(s_manage), "wifi_task", 3096, this, 5, &m_task);
  }

  ~WifiManager() override;
  int connect() override;
  int disconnect() override;

  [[noreturn]] void manage();

  enum class wifi_state { connect, connecting, connected, disconnect, disconnected, broadcast, broadcasting };

private:
  void update_state(wifi_state state);
  [[noreturn]] static void s_manage(WifiManager *that);
  int init_connection();
  int handle_connecting();
  int handle_disconnect();
  int handle_broadcasting();
  int init_softap();

  static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

  ConfigManager& m_config_manager;
  SemaphoreHandle_t m_mutex;
  wifi_state m_state;
  TaskHandle_t m_task = nullptr;
  unsigned int m_attempts = 0;
  esp_netif_t *m_netif = nullptr;

};

#endif //NETWORKMANAGER_H
