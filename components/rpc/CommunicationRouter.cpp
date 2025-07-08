#include "CommunicationRouter.h"

#include <iostream>

CommunicationRouter::~CommunicationRouter() {
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
