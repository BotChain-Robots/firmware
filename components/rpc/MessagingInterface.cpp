//
// Created by Johnathon Slightham on 2025-05-25.
//

#include "MessagingInterface.h"

#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "MPIMessageBuilder.h"

MessagingInterface::~MessagingInterface() {

}

int MessagingInterface::send(char* buffer, int size, int destination, int tag, bool durable) {
    return 0;
}

int MessagingInterface::broadcast(char* buffer, int size, int root, bool durable) {

    return 0;
}

int MessagingInterface::recv(char* buffer, int size, int source, const int tag) {
    checkOrInsertTag(tag);

    // todo: the buffer needs to be large enough, this copies into the buffer...
    // todo: handle the source
    xQueueReceive(m_tag_to_queue.at(tag), buffer, portMAX_DELAY);

    return 0;
}

int MessagingInterface::sendrecv(char* send_buffer, int send_size, int dest, int send_tag, char* recv_buffer, int recv_size, int recv_tag) {

    return 0;
}

// todo: when handleRecv returns, remove from queue (from router)
void MessagingInterface::handleRecv(const char* recv_buffer, int recv_size) {
    const auto mpi_message = Flatbuffers::MPIMessageBuilder::parse_mpi_message(reinterpret_cast<const uint8_t *>(recv_buffer));

    checkOrInsertTag(mpi_message->tag());

    xQueueSendToBack(m_tag_to_queue.at(mpi_message->tag()), mpi_message->payload(), 0);
}

void MessagingInterface::checkOrInsertTag(const uint8_t tag) {
    xSemaphoreTake(m_map_semaphore, portMAX_DELAY);
    if (!m_tag_to_queue.contains(tag)) {
        m_tag_to_queue[tag] = xQueueCreate(MPI_QUEUE_SIZE, MAX_MPI_BUFFER_SIZE);
    }
    xSemaphoreGive(m_map_semaphore);
}
