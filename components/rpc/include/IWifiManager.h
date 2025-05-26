//
// Created by Johnathon Slightham on 2025-05-26.
//

#ifndef IWIFIMANAGER_H
#define IWIFIMANAGER_H

class IWifiManager {
public:
    virtual ~IWifiManager() {};
    virtual int connect();
    virtual int disconnect();
};

#endif //IWIFIMANAGER_H
