//mdns and other stuff main file
#include <cstdio>
#include <memory>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "WifiManager.h"
#include "mDNSDiscoveryService.h"
#include "TCPServer.h"
#include "ConfigManager.h"
#include "constants/tcp.h"
#include "LoopManager.h"
#include "esp_log.h"

extern "C" [[noreturn]] void app_main(void) {
    ESP_LOGI("MEM", "Free internal RAM: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI("MEM", "Free PSRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    ConfigManager::init_config();

    const auto manager = std::make_unique<WifiManager>();
    manager->connect();

    mDNSDiscoveryService::setup();

    LoopManager::control_loop();
}
