//
// Created by Johnathon Slightham on 2025-05-25.
//

#include "mdns.h"

#include "ConfigManager.h"

#include "mDNSDiscoveryService.h"

#include "constants/tcp.h"

#include <string>
#include <format>

// todo: clean this up (strange to be a constructor) also need to add more details
mDNSDiscoveryService::mDNSDiscoveryService() {
    mdns_init();
    mdns_hostname_set(std::format("botchain-{}", ConfigManager::get_module_id()).c_str());
    mdns_instance_name_set(std::format("BotChain Robot Module {}", ConfigManager::get_module_id()).c_str());

    mdns_service_add(nullptr, "_robotcontrol", "_tcp", TCP_PORT, nullptr, 0);
    mdns_service_instance_name_set("_robotcontrol", "_tcp", "Robot Control TCP Server");
}
