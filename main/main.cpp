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

    ConfigManager::set_module_type(ModuleType_DC_MOTOR);
    ConfigManager::set_module_id(9);

    LoopManager::control_loop();
}
