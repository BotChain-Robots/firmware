//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef RPCFACTORY_H
#define RPCFACTORY_H

class RPCFactory {
public:
static std::shared_ptr<IRPCServer> createRPCServer() {
    return std::make_shared<TCPServer>();
}

#endif //RPCFACTORY_H
