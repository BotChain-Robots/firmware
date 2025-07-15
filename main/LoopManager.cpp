//
// Created by Johnathon Slightham on 2025-07-05.
//

#include "LoopManager.h"

#include <iostream>
#include <memory>
#include <MessagingInterface.h>

#include "esp_log.h"

#define ACTUATOR_CMD_TAG 5

[[noreturn]] void LoopManager::control_loop() {
    const auto messaging_interface = std::make_unique<MessagingInterface>(std::make_unique<WifiManager>());

    const auto actuator = ActuatorFactory::create_actuator(ConfigManager::get_module_type());

    uint8_t buffer[512];
    while (true) {
        messaging_interface->recv(reinterpret_cast<char *>(buffer), 512, PC_ADDR, ACTUATOR_CMD_TAG);
        actuator->actuate(buffer);
    }
}
