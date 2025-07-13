//
// Created by Johnathon Slightham on 2025-05-25.
//

#include "mdns.h"

#include "ConfigManager.h"

#include "mDNSDiscoveryService.h"

#include "constants/tcp.h"

#include <string>
#include <format>
#include <sstream>

// todo: clean this up (strange to be a constructor) also need to add more details, need to add to routing table
void mDNSDiscoveryService::setup() {
    mdns_init();
    mdns_hostname_set(std::format("botchain-{}", ConfigManager::get_module_id()).c_str());
    mdns_instance_name_set(std::format("BotChain Robot Module {}", ConfigManager::get_module_id()).c_str());

    mdns_service_add(nullptr, "_robotcontrol", "_tcp", TCP_PORT, nullptr, 0);
    mdns_service_instance_name_set("_robotcontrol", "_tcp", "Robot Control TCP Server");

    mdns_txt_item_t service_txt_data[3] = {
        {"module_id",std::to_string(ConfigManager::get_module_id()).c_str()},
        {"module_type",std::to_string(ConfigManager::get_module_type()).c_str()},
           {"connected_modules",""},
    };

    mdns_service_txt_set("_robotcontrol", "_tcp", service_txt_data, 3);
}

void mDNSDiscoveryService::set_connected_boards(std::vector<int>& boards) {
    std::stringstream ss;

    for (int i = 0; i < boards.size(); i++) {
        ss << boards[i];
        if (i < boards.size() - 1) {
            ss << ',';
        }
    }

    mdns_txt_item_t service_txt_data[3] = {
        {"module_id",std::to_string(ConfigManager::get_module_id()).c_str()},
        {"module_type",std::to_string(ConfigManager::get_module_type()).c_str()},
           {"connected_modules",ss.str().c_str()},
    };
    mdns_service_txt_set("_robotcontrol", "_tcp", service_txt_data, 3);
}
