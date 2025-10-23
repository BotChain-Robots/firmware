//
// Created by Johnathon Slightham on 2025-10-22.
//

#include <memory>

#include "CommunicationFactory.h"
#include "constants/tcp.h"
#include "wireless/mDNSDiscoveryService.h"
#include "wireless/TCPServer.h"
#include "wireless/WifiManager.h"

std::unique_ptr<IConnectionManager> CommunicationFactory::create_connection_manager(const CommunicationMethod type) {
    switch (type) {
        case Wireless:
            return std::make_unique<WifiManager>();
        default:
            return nullptr;
    }
}

std::unique_ptr<IDiscoveryService> CommunicationFactory::create_discovery_service(const CommunicationMethod type) {
    switch (type) {
        case Wireless:
            return std::make_unique<mDNSDiscoveryService>();
        default:
            return nullptr;
    }
}

std::unique_ptr<IRPCServer> CommunicationFactory::create_lossy_server(const CommunicationMethod type, const std::shared_ptr<PtrQueue<std::vector<uint8_t>>>& rx_queue) {
    switch (type) {
        case Wireless:
            return std::make_unique<TCPServer>(TCP_PORT, rx_queue); // todo: replace with udp server
        default:
            return nullptr;
    }
}

std::unique_ptr<IRPCServer> CommunicationFactory::create_lossless_server(const CommunicationMethod type, const std::shared_ptr<PtrQueue<std::vector<uint8_t>>>& rx_queue) {
    switch (type) {
        case Wireless:
            return std::make_unique<TCPServer>(TCP_PORT, rx_queue);
        default:
            return nullptr;
    }
}
