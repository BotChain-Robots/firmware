//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef UDPSERVER_H
#define UDPSERVER_H

#include <memory>
#include <unordered_set>
#include <vector>

#include "IRPCServer.h"
#include "BlockingQueue.h"
#include "freertos/FreeRTOS.h"

class UDPServer final : public IRPCServer {
  public:
    UDPServer(int rx_port, int tx_port,
              const std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> &rx_queue);
    ~UDPServer() override;
    void startup() override;
    void shutdown() override;
    int send_msg(uint8_t *buffer, size_t size) const override;

  private:
    bool authenticate_client(int client_sock);

    static bool is_network_connected();
    [[noreturn]] static void socket_monitor_thread(void *args);

    int m_tx_port;
    int m_rx_port;

    int m_tx_server_sock;
    int m_rx_server_sock;

    TaskHandle_t m_rx_task;

    std::shared_ptr<BlockingQueue<std::unique_ptr<std::vector<uint8_t>>>> m_rx_queue;
};

#endif //UDPSERVER_H
