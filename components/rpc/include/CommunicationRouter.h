//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef COMMUNICATIONROUTER_H
#define COMMUNICATIONROUTER_H

#include <functional>
#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "TCPServer.h"
#include "constants/tcp.h"
#include "constants/app_comms.h"

class CommunicationRouter {
public:
    explicit CommunicationRouter(const std::function<void(char*, int)> &rx_callback)
        : m_tcp_rx_queue(xQueueCreate(RX_QUEUE_SIZE, MAX_RX_BUFFER_SIZE)),
            m_rx_callback(rx_callback),
            m_tcp_server(std::make_unique<TCPServer>(TCP_PORT, m_tcp_rx_queue)) {
        xTaskCreate(router_thread, "communication_router", 2048, this, 3, &this->m_router_thread);
    }

    ~CommunicationRouter();

    [[noreturn]] static void router_thread(void *args);

    // todo: does this really need to be here (so i can access from thread)?
    QueueHandle_t m_tcp_rx_queue;
    std::function<void(char*, int)> m_rx_callback;
private:

    TaskHandle_t m_router_thread;
    std::unique_ptr<TCPServer> m_tcp_server;

};

#endif //COMMUNICATIONROUTER_H
