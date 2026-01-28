//
// Created by Johnathon Slightham on 2025-05-25.
//

#include <ranges>

#include "MessagingInterface.h"
#include "AngleControlMessageBuilder.h"
#include "ConfigManager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "MPIMessageBuilder.h"

MessagingInterface::~MessagingInterface() {
    vSemaphoreDelete(m_map_semaphore);

    for (const auto queue: m_tag_to_queue | std::views::values) {
        vQueueDelete(queue);
    }
}

int MessagingInterface::send(char* buffer, const int size, const int destination, const int tag, const bool durable) {
    Flatbuffers::MPIMessageBuilder builder;
    const auto [mpi_buffer, mpi_size] = builder.build_mpi_message(Messaging::MessageType_PTP, m_config_manager.get_module_id(), destination, m_sequence_number++, durable, tag, std::vector<uint8_t>(buffer, buffer + size));

    m_router->send_msg(static_cast<char *>(mpi_buffer), mpi_size);
    return 0;
}

int MessagingInterface::broadcast(char* buffer, int size, int root, bool durable) {
    // todo: impl
    return 0;
}

int MessagingInterface::recv(char* buffer, int size, int source, const int tag) {
    checkOrInsertTag(tag);

    // todo: the buffer needs to be large enough, this copies into the buffer...
    // todo: handle the source
    xQueueReceive(m_tag_to_queue.at(tag), buffer, portMAX_DELAY);

    return 0;
}

int MessagingInterface::sendrecv(char* send_buffer, const int send_size, const int dest, const int send_tag, char* recv_buffer, const int recv_size, const int recv_tag, const bool durable) {
    send(send_buffer, send_size, dest, send_tag, durable);
    recv(recv_buffer, recv_size, dest, recv_tag);

    return 0;
}

// todo: when handleRecv returns, remove from queue (from router)
void MessagingInterface::handleRecv(std::unique_ptr<std::vector<uint8_t>>&& buffer) {
    const auto mpi_message = Flatbuffers::MPIMessageBuilder::parse_mpi_message(buffer->data());

    checkOrInsertTag(mpi_message->tag());

    xQueueSendToBack(m_tag_to_queue.at(mpi_message->tag()), mpi_message->payload()->data(), 0);
}

void MessagingInterface::checkOrInsertTag(const uint8_t tag) {
    xSemaphoreTake(m_map_semaphore, portMAX_DELAY);
    if (!m_tag_to_queue.contains(tag)) {
        m_tag_to_queue[tag] = xQueueCreate(MPI_QUEUE_SIZE, MAX_MPI_BUFFER_SIZE);
    }
    xSemaphoreGive(m_map_semaphore);
}

std::pair<std::vector<uint8_t>, std::vector<Orientation>> MessagingInterface::get_physically_connected_modules() const {
    return m_router->get_physically_connected_modules();
}

Messaging::ConnectionType MessagingInterface::get_connection_type() const {
    if (this->m_router->get_leader() == m_config_manager.get_module_id()) {
        return Messaging::ConnectionType_DIRECT;
    }
    return Messaging::ConnectionType_HOP;
}

uint8_t MessagingInterface::get_leader() const {
    return this->m_router->get_leader();
}
