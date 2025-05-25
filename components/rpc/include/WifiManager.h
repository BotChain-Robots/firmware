#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class WifiManager {
public:
  WifiManager() = default;
  ~WifiManager() = default;
  int init();
  int connect();
  int disconnect();

  [[noreturn]] void manage();

  enum class wifi_state { connect, connected, disconnected, connecting, disconnect, searching };

private:
  void update_state(wifi_state state);
  [[noreturn]] static void s_manage(WifiManager *that);
  int init_connection();
  int handle_connecting();
  int handle_disconnect();
  int handle_search();

  static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);


  SemaphoreHandle_t m_mutex;
  wifi_state m_state;
  TaskHandle_t m_task;
  unsigned int m_attempts;

};

#endif //NETWORKMANAGER_H
