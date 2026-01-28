//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef IRPCSERVER_H
#define IRPCSERVER_H

#include <memory>
#include <vector>

class IRPCServer {
  public:
    virtual ~IRPCServer() = default;
    virtual void startup() = 0;
    virtual void shutdown() = 0;
    virtual int send_msg(uint8_t *buffer, size_t size) const = 0;
};

#endif //IRPCSERVER_H
