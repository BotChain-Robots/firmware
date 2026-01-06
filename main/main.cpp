#if !defined(RMT_TEST) || (defined(RMT_TEST) && RMT_TEST == 0)
// #include <cstdio>
// #include <memory>

#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"
#include "ConfigManager.h"
#include "LoopManager.h"
#include "esp_log.h"

extern "C" [[noreturn]] void app_main(void) {
    ESP_LOGI("MEM", "Free internal RAM: %d",
        heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI("MEM", "Free PSRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    auto& config_manager = ConfigManager::get_instance(); // NOLINT - here for easily adding temporary config

    const auto loop_manager = std::make_unique<LoopManager>();
    xTaskCreate(reinterpret_cast<TaskFunction_t>(LoopManager::metadata_tx_loop),
        "metadata_tx", 3096, loop_manager.get(), 3, nullptr);
    loop_manager->control_loop();
}
#endif