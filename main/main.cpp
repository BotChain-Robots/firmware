#include <cstdio>
#include <memory>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "WifiManager.h"
#include "mDNSDiscoveryService.h"
#include "TCPServer.h"
#include "ConfigManager.h"
#include "constants/tcp.h"

extern "C" [[noreturn]] void app_main(void) {
    ConfigManager::init_config();

    const auto manager = std::make_unique<WifiManager>();
    manager->connect();

    mDNSDiscoveryService::setup();

    const auto tcp_server = std::make_unique<TCPServer>(TCP_PORT);

    printf("Hello world!\n");

    for (int i = 0; ; i++) {
        printf("Beat %d\n", i);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
