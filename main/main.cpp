#include <cstdio>
#include <memory>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_flash.h"
#include "nvs_flash.h"

#include "WifiManager.h"
#include "mDNSDiscoveryService.h"

extern "C" [[noreturn]] void app_main(void) {
    nvs_flash_init();

    const auto manager = std::make_unique<WifiManager>();
    manager->connect();

    const auto discovery = std::make_unique<mDNSDiscoveryService>();

    printf("Hello world!\n");

    for (int i = 0; ; i++) {
        printf("Beat %d\n", i);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
