//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <memory>
#include <unordered_set>
#include <vector>

#include "IRPCServer.h"
#include "BlockingQueue.h"
#include "freertos/FreeRTOS.h"

class TCPServer final : public IRPCServer {
  public:
    TCPServer(int port, const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue);
    ~TCPServer() override;
    void startup() override;
    void shutdown() override;
    int send_msg(uint8_t* buffer, size_t size) const override;

  private:
    bool authenticate_client(int client_sock);

    static bool is_network_connected();
    [[noreturn]] static void tcp_server_task(void *args);
    [[noreturn]] static void socket_monitor_thread(void *args);

    int m_port;
    int m_server_sock;

    TaskHandle_t m_task;
    TaskHandle_t m_rx_task;

    std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> m_rx_queue;

    SemaphoreHandle_t m_mutex;
    std::unordered_set<int> m_clients;
};

#endif //TCPSERVER_H
