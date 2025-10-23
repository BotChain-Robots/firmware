//
// Created by Johnathon Slightham on 2025-05-26.
//

#ifndef ICONNECTIONMANAGER_H
#define ICONNECTIONMANAGER_H

class IConnectionManager{
public:
    virtual ~IConnectionManager() = default;
    virtual int connect() = 0;
    virtual int disconnect() = 0;
};

#endif //IWIFIMANAGER_H
