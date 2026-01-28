#include <chrono>
#include <cstring>
#include <iostream>

#include "AngleControlMessageBuilder.h"
#include "CommunicationRouter.h"
#include "MPIMessageBuilder.h"
#include "OrientationDetection.h"
#include "PtrQueue.h"
#include "Tables.h"
#include "constants/module.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "include/wireless/WifiManager.h"
#include "include/wireless/mDNSDiscoveryService.h"

#define TAG "CommunicationRouter"
#define MAX_RX_BUFFER_SIZE 1024
#define WIRELESS_DEQUEUE_TIMEOUT_MS 3000

CommunicationRouter::~CommunicationRouter() {
    vTaskDelete(m_router_thread);
}

[[noreturn]] void CommunicationRouter::router_thread(void *args) {
    const auto that = static_cast<CommunicationRouter *>(args);

    while (true) {
        if( auto maybe_buffer = that->m_tcp_rx_queue->dequeue(std::chrono::milliseconds(WIRELESS_DEQUEUE_TIMEOUT_MS))) {
            ESP_LOGD(TAG, "Got message from TCP");
            that->route(std::move(*maybe_buffer));
        }
    }
}

[[noreturn]] void CommunicationRouter::link_layer_thread(void *args) {
    const auto that = static_cast<CommunicationRouter *>(args);

    while (true) {
        if (std::chrono::system_clock::now() - that->m_last_leader_updated >
            std::chrono::seconds(2)) {
            that->m_last_leader_updated = std::chrono::system_clock::now();
            that->update_leader();
        }

        if (auto ptr = that->m_data_link_manager->async_receive()) {
            that->route(std::move(*ptr));
        }
    }
}

int CommunicationRouter::send_msg(char *buffer, const size_t length) const {
    route(reinterpret_cast<uint8_t *>(buffer), length);
    return 0;
}

void CommunicationRouter::update_leader() {
    RIPRow_public table[RIP_MAX_ROUTES];
    size_t table_size = RIP_MAX_ROUTES;
    this->m_data_link_manager->get_routing_table(table, &table_size);

    // Leader election (just get the highest id in rip)
    std::vector<int> connected_module_ids;
    uint8_t max = m_module_id;
    for (int i = 0; i < table_size; i++) {
        const auto id = table[i].info.board_id;
        connected_module_ids.emplace_back(id);
        if (id > max) { // todo: change this to be correct
            max = id;
        }
    }

    // Leader has changed, we may need to change PC connection state
    if (this->m_leader != max) {
        ESP_LOGI(TAG, "Leader has changed from %d to %d", this->m_leader, max);
        if (max == m_module_id) {
            m_pc_connection->connect();
            m_lossless_server->startup();
            m_lossy_server->startup();
        } else if (this->m_leader == m_module_id) {
            m_pc_connection->disconnect();
            m_lossless_server->shutdown();
            m_lossy_server->shutdown();
        }
    }

    this->m_leader = max;

    if (this->m_leader == m_module_id) {
        this->m_discovery_service->set_connected_boards(connected_module_ids);
    }
}

// Route without trying to copy to heap. Only call if you do not have a unique_ptr.
// To handle the case of writing directly from control -> TCP/UDP, nothing has to touch the heap.
void CommunicationRouter::route(uint8_t *buffer, size_t size) const {
    flatbuffers::Verifier verifier(buffer, size);
    // This could be moved to just be called on wireline data to save cpu cycles.
    if (bool ok = Messaging::VerifyMPIMessageBuffer(verifier); !ok) {
        ESP_LOGW(TAG, "route: got an invalid MPI message, disregarding");
        return;
    }

    if (const auto &mpi_message = Flatbuffers::MPIMessageBuilder::parse_mpi_message(buffer);
        mpi_message->destination() == m_module_id) {
        auto ubuffer = std::make_unique<std::vector<uint8_t>>();
        ubuffer->resize(size);
        memcpy(ubuffer->data(), buffer, size);
        this->m_rx_callback(std::move(ubuffer));
    } else if (mpi_message->destination() == PC_ADDR && this->m_leader == m_module_id) {
        if (mpi_message->is_durable()) {
            this->m_lossless_server->send_msg(buffer, size);
        } else {
            this->m_lossy_server->send_msg(buffer, size);
        }
    } else {
        const auto dest = mpi_message->destination() == PC_ADDR ? this->m_leader : mpi_message->destination();

        auto u_buffer = std::make_unique<std::vector<uint8_t>>();
        u_buffer->resize(size);
        memcpy(u_buffer->data(), buffer, size);

        this->m_data_link_manager->send(dest, std::move(u_buffer), FrameType::MOTOR_TYPE, 0);
    }
}

// Route heap messages
void CommunicationRouter::route(std::unique_ptr<std::vector<uint8_t>>&& buffer) const {
    flatbuffers::Verifier verifier(buffer->data(), buffer->size());
    // This could be moved to just be called on wireline data to save cpu cycles.
    if (bool ok = Messaging::VerifyMPIMessageBuffer(verifier); !ok) {
        ESP_LOGW(TAG, "route: got an invalid MPI message, disregarding");
        return;
    }

    if (const auto &mpi_message = Flatbuffers::MPIMessageBuilder::parse_mpi_message(buffer->data());
        mpi_message->destination() == m_module_id) {
        this->m_rx_callback(std::move(buffer));
    } else if (mpi_message->destination() == PC_ADDR && this->m_leader == m_module_id) {
        if (mpi_message->is_durable()) {
            this->m_lossless_server->send_msg(buffer->data(), buffer->size());
        } else {
            this->m_lossy_server->send_msg(buffer->data(), buffer->size());
        }
    } else if (mpi_message->destination() == PC_ADDR) {
        this->m_data_link_manager->send(this->m_leader, std::move(buffer), FrameType::MOTOR_TYPE, 0);
    } else {
        this->m_data_link_manager->send(mpi_message->destination(), std::move(buffer), FrameType::MOTOR_TYPE,
                                        0);
    }
}

std::pair<std::vector<uint8_t>, std::vector<Orientation>>
CommunicationRouter::get_physically_connected_modules() const {
    std::vector<RIPRow_public> table;
    table.resize(RIP_MAX_ROUTES);
    size_t table_size = RIP_MAX_ROUTES * sizeof(RIPRow_public);
    m_data_link_manager->get_routing_table(table.data(), &table_size);

    std::vector<uint8_t> connected_module_ids;
    std::vector<Orientation> connected_module_orientations;
    connected_module_ids.resize(MAX_WIRED_CONNECTIONS);
    connected_module_orientations.resize(MAX_WIRED_CONNECTIONS);

    for (int i = 0; i < MAX_WIRED_CONNECTIONS; i++) {
        connected_module_ids[i] = 0; // this is not the PC ID here, marking as nc.
    }

    for (int i = 0; i < table_size; i++) {
        if (table[i].info.hops == 1 && table[i].channel < MAX_WIRED_CONNECTIONS) {
            connected_module_ids[table[i].channel] = table[i].info.board_id;
        }
    }

    if (const auto id = connected_module_ids[0]; 0 == id) {
        connected_module_orientations[0] = Orientation_Deg0;
    } else {
        connected_module_orientations[0] = OrientationDetection::get_orientation(0);
    }

    return {connected_module_ids, connected_module_orientations};
}

[[nodiscard]] uint8_t CommunicationRouter::get_leader() const {
    return this->m_leader;
}
