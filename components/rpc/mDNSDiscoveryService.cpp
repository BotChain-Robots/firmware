//
// Created by Johnathon Slightham on 2025-05-25.
//

#include "mdns.h"

#include "mDNSDiscoveryService.h"

mDNSDiscoveryService::mDNSDiscoveryService() {
    mdns_init();
    mdns_hostname_set("botchain-0000");
    mdns_instance_name_set("BotChain Robot Module 0000");

    mdns_service_add(NULL, "_robotcontrol", "_udp", 3000, NULL, 0);
    mdns_service_instance_name_set("_robotcontrol", "_udp", "Robot Control UDP Server");
}
