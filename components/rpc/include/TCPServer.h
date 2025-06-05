//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <vector>
#include <unordered_set>

#include "freertos/FreeRTOS.h"

#include "IRPCServer.h"

class TCPServer : IRPCServer {
public:
    explicit TCPServer(int port);
    ~TCPServer();

private:
    bool authenticate_client(int client_sock);

    static bool is_network_connected();
    [[noreturn]] static void tcp_server_task(void *args);
    [[noreturn]] static void socket_monitor_thread(void *args);

    int m_port;
    int m_server_sock;

    TaskHandle_t m_task;
    TaskHandle_t m_rx_task;
    TaskHandle_t m_tx_task;

    SemaphoreHandle_t m_mutex;
    std::unordered_set<int> m_clients;
};

#endif //TCPSERVER_H
