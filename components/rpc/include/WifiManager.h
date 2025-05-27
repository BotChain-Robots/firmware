#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <esp_netif_types.h>

#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "IWifiManager.h"

class WifiManager final : IWifiManager {
public:
  WifiManager();
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
  int init_softap();

  static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

  SemaphoreHandle_t m_mutex;
  wifi_state m_state;
  TaskHandle_t m_task;
  unsigned int m_attempts;
  esp_netif_t *m_netif;

};

#endif //NETWORKMANAGER_H
