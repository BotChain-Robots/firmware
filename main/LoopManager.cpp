//
// Created by Johnathon Slightham on 2025-07-05.
//

#include "LoopManager.h"

#include <iostream>
#include <memory>
#include <MessagingInterface.h>

#include "esp_log.h"

[[noreturn]] void LoopManager::control_loop() {
    const auto messaging_interface = std::make_unique<MessagingInterface>(std::make_unique<WifiManager>());

    char buffer[512];
    while (true) {
        messaging_interface->recv(buffer, 512, 0, 1);
        //std::cout << buffer << std::endl;

        std::string s = std::format("num {} bo", ConfigManager::get_module_id());
        messaging_interface->send(s.data(), s.size(), 0, 2, true);

        ESP_LOGI("MEM", "Free internal RAM: %d", heap_caps_get_free_size(MALLOC_CAP_8BIT));
        ESP_LOGI("MEM", "Free PSRAM: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    }
}
