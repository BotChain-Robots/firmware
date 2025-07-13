//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef DISCOVERYSERVICE_H
#define DISCOVERYSERVICE_H

#include <vector>

class mDNSDiscoveryService final {
public:
    mDNSDiscoveryService() = delete;
    ~mDNSDiscoveryService() = delete;

    static void setup();
    static void set_connected_boards(std::vector<int>& boards);
};

#endif //DISCOVERYSERVICE_H
