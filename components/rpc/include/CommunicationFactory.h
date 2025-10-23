//
// Created by Johnathon Slightham on 2025-10-22.
//

// Although we currently only support wireless, this is architected to be easy to add other connection methods

#ifndef COMMUNICATIONFACTORY_H
#define COMMUNICATIONFACTORY_H

#include <memory>
#include <vector>

#include "IConnectionManager.h"
#include "IDiscoveryService.h"
#include "IRPCServer.h"
#include "PtrQueue.h"
#include "enums.h"

class CommunicationFactory {
public:
    static std::unique_ptr<IConnectionManager> create_connection_manager(CommunicationMethod type);
    static std::unique_ptr<IDiscoveryService> create_discovery_service(CommunicationMethod type);
    static std::unique_ptr<IRPCServer> create_lossy_server(CommunicationMethod type, const std::shared_ptr<PtrQueue<std::vector<uint8_t>>> &rx_queue);
    static std::unique_ptr<IRPCServer> create_lossless_server(CommunicationMethod type, const std::shared_ptr<PtrQueue<std::vector<uint8_t>>>& rx_queue);
};

#endif //COMMUNICATIONFACTORY_H
