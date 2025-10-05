//
// Created by Johnathon Slightham on 2025-07-05.
//

#ifndef LOOPMANAGER_H
#define LOOPMANAGER_H

#include <memory>

#include <MessagingInterface.h>

#include "control/IActuator.h"
#include "control/ActuatorFactory.h"

class LoopManager {
public:
    LoopManager() : m_config_manager(ConfigManager::get_instance()),
        m_messaging_interface(std::make_unique<MessagingInterface>(std::make_unique<WifiManager>())) {}
    [[noreturn]] void control_loop() const;
    [[noreturn]] static void metadata_tx_loop(char * args);
    [[noreturn]] static void metadata_rx_loop(char * args);

private:
    ConfigManager& m_config_manager;
    std::unique_ptr<MessagingInterface> m_messaging_interface;

private:

};



#endif //LOOPMANAGER_H
