//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef IRPCSERVER_H
#define IRPCSERVER_H

class IRPCServer {
public:
    virtual ~IRPCServer() = default;
    virtual int send_msg(char* buffer, uint32_t length) const = 0;
};

#endif //IRPCSERVER_H
