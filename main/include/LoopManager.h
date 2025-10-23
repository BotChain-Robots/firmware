//
// Created by Johnathon Slightham on 2025-07-05.
//

#ifndef LOOPMANAGER_H
#define LOOPMANAGER_H

#include <memory>

#include "MessagingInterface.h"

class LoopManager {
public:
    LoopManager() : m_config_manager(ConfigManager::get_instance()),
        m_messaging_interface(std::make_unique<MessagingInterface>()) {}
    [[noreturn]] void control_loop() const;                     // gets control commands
    [[noreturn]] void sensor_loop() const;                      // sends sensor data commands continually
    [[noreturn]] static void metadata_tx_loop(char * args);     // sends metadata continually (low duty cycle)
    [[noreturn]] static void metadata_rx_loop(char * args);     // gets other commands from PC (ie. f/w updates, nvs updates)

private:
    ConfigManager& m_config_manager;
    std::unique_ptr<MessagingInterface> m_messaging_interface;
};

#endif //LOOPMANAGER_H
