//
// Created by Johnathon Slightham on 2025-05-26.
//

#ifndef IDISCOVERYSERVICE_H
#define IDISCOVERYSERVICE_H

class IDiscoveryService {
public:
    virtual ~IDiscoveryService() = default;

    virtual void set_connected_boards(const std::vector<int>& boards) = 0;
};

#endif //IDISCOVERYSERVICE_H
