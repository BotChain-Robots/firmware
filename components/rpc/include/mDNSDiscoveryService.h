//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef DISCOVERYSERVICE_H
#define DISCOVERYSERVICE_H

class mDNSDiscoveryService final {
public:
    mDNSDiscoveryService() = delete;
    ~mDNSDiscoveryService() = delete;

    static void setup();
};

#endif //DISCOVERYSERVICE_H
