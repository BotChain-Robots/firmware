//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef DISCOVERYSERVICE_H
#define DISCOVERYSERVICE_H

#include <vector>

#include "IDiscoveryService.h"

class mDNSDiscoveryService final : public IDiscoveryService {
public:
    mDNSDiscoveryService();
    ~mDNSDiscoveryService() override;

    void set_connected_boards(const std::vector<int>& boards) override;
};

#endif //DISCOVERYSERVICE_H
