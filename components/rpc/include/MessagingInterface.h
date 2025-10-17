//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef MESSAGINGINTERFACE_H
#define MESSAGINGINTERFACE_H

#include <memory>
#include <unordered_map>
#include <flatbuffers_generated/TopologyMessage_generated.h>

#include "constants/app_comms.h"
#include "CommunicationRouter.h"

class MessagingInterface {
public:
    explicit MessagingInterface(std::unique_ptr<WifiManager>&& pc_connection)
        : m_config_manager(ConfigManager::get_instance()),
            m_mpi_rx_queue(xQueueCreate(MAX_RX_BUFFER_SIZE, RX_QUEUE_SIZE)),
            m_router(std::make_unique<CommunicationRouter>([this](const char* buffer, const int size) { handleRecv(buffer, size); }, std::move(pc_connection))),
            m_map_semaphore(xSemaphoreCreateMutex()) {};

    ~MessagingInterface();

    int send(char* buffer, int size, int destination, int tag, bool durable);
    int broadcast(char* buffer, int size, int root, bool durable);
    int recv(char* buffer, int size, int source, int tag);
    int sendrecv(char *send_buffer, int send_size, int dest, int send_tag, char *recv_buffer, int recv_size, int recv_tag, bool durable);
    std::pair<std::vector<uint8_t>, std::vector<Orientation>> get_physically_connected_modules() const;
    Messaging::ConnectionType get_connection_type() const;
    uint8_t get_leader() const;

private:
    void handleRecv(const char* recv_buffer, int recv_size);

    void checkOrInsertTag(uint8_t tag);

    ConfigManager& m_config_manager;
    uint16_t m_sequence_number = 0;
    QueueHandle_t m_mpi_rx_queue; // todo: maybe move this down classes more
    std::unique_ptr<CommunicationRouter> m_router;
    SemaphoreHandle_t m_map_semaphore;
    std::unordered_map<uint8_t, QueueHandle_t> m_tag_to_queue;
};

#endif //MESSAGINGINTERFACE_H
