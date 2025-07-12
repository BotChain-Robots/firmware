#include "CommunicationRouter.h"

#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

CommunicationRouter::~CommunicationRouter() {
    vTaskDelete(m_router_thread);
    vQueueDelete(m_tcp_rx_queue);
}

[[noreturn]] void CommunicationRouter::router_thread(void *args) {
    const auto that = static_cast<CommunicationRouter *>(args);

    // todo: change to queue set
    char buffer[512];
    while (true) {
        xQueueReceive(that->m_tcp_rx_queue, buffer, portMAX_DELAY);
        that->m_rx_callback(buffer, 512);
        std::cout << "callback" << std::endl;
    }
}

int CommunicationRouter::send_msg(char* buffer, const size_t length) const {
    return this->m_tcp_server->send_msg(buffer, length);
}
