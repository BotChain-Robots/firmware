//mdns and other stuff main file
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
#include "LoopManager.h"

extern "C" [[noreturn]] void app_main(void) {
    ConfigManager::init_config();

    const auto manager = std::make_unique<WifiManager>();
    manager->connect();

    mDNSDiscoveryService::setup();

    LoopManager::control_loop();
}
