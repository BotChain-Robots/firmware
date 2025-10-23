//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <memory>
#include <vector>
#include <unordered_set>

#include "freertos/FreeRTOS.h"
#include "IRPCServer.h"
#include "PtrQueue.h"

class TCPServer final : public IRPCServer {
public:
    TCPServer(int port, const std::shared_ptr<PtrQueue<std::vector<uint8_t>>>& rx_queue);
    ~TCPServer() override;
    int send_msg(char* buffer, uint32_t length) const override;

private:
    bool authenticate_client(int client_sock);

    static bool is_network_connected();
    [[noreturn]] static void tcp_server_task(void *args);
    [[noreturn]] static void socket_monitor_thread(void *args);

    int m_port;
    int m_server_sock;

    TaskHandle_t m_task;
    TaskHandle_t m_rx_task;

    std::shared_ptr<PtrQueue<std::vector<uint8_t>>> m_rx_queue;

    SemaphoreHandle_t m_mutex;
    std::unordered_set<int> m_clients;
};

#endif //TCPSERVER_H
