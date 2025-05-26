//
// Created by Johnathon Slightham on 2025-05-26.
//

#ifndef IWIFIMANAGER_H
#define IWIFIMANAGER_H

class IWifiManager {
public:
    virtual ~IWifiManager() = default;
    virtual int connect() = 0;
    virtual int disconnect() = 0;
};

#endif //IWIFIMANAGER_H
