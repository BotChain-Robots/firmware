#include <cstdio>
#include <memory>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"

#include "WifiManager.h"
#include "mDNSDiscoveryService.h"
#include "TCPServer.h"
#include "ConfigManager.h"
#include "LoopManager.h"
#include "esp_log.h"

extern "C" [[noreturn]] void app_main(void) {
    ESP_LOGI("MEM", "Free internal RAM: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI("MEM", "Free PSRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    ConfigManager::init_config();



    const auto loop_manager = std::make_unique<LoopManager>();
    xTaskCreate(reinterpret_cast<TaskFunction_t>(LoopManager::metadata_tx_loop), "metadata_tx", 3096, loop_manager.get(), 3, nullptr);
    loop_manager->control_loop();
}
