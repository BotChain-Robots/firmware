#include <iostream>

#include "CommunicationRouter.h"
#include "AngleControlMessageBuilder.h"
#include "include/wireless/mDNSDiscoveryService.h"
#include "MPIMessageBuilder.h"
#include "include/wireless/WifiManager.h"
#include "freertos/FreeRTOS.h"
#include "Tables.h"
#include "PtrQueue.h"
#include "OrientationDetection.h"

#define TAG "CommunicationRouter"

CommunicationRouter::~CommunicationRouter() {
    vTaskDelete(m_router_thread);
}

// todo: we really need to change all char to uint8_t everywhere
// todo: get rid of copying going on, need to pass around sharedptrs/uniqueptrs

[[noreturn]] void CommunicationRouter::router_thread(void *args) {
    const auto that = static_cast<CommunicationRouter *>(args);

    while (true) {
        const auto buffer = that->m_tcp_rx_queue->dequeue();
        ESP_LOGD(TAG, "Got message from TCP");
        that->route(buffer->data(), buffer->size());
    }
}

[[noreturn]] void CommunicationRouter::link_layer_thread(void *args) {
    const auto* params = static_cast<link_layer_thread_params *>(args);
    const auto that = params->router;
    const auto channel = params->channel;
    delete params;

    size_t bytes_received = 0;
    that->m_data_link_manager->start_receive_frames(channel);
    while (true) {
        char buffer[512];
        const auto err = that->m_data_link_manager->receive(reinterpret_cast<uint8_t *>(buffer), 512, &bytes_received, channel);
        that->m_data_link_manager->start_receive_frames(channel);

        // todo: do we only want one thread ever doing this?
        if (std::chrono::system_clock::now() - that->m_last_leader_updated > std::chrono::seconds(15)) {
            that->m_last_leader_updated = std::chrono::system_clock::now();
            ESP_LOGI(TAG, "Updating leader");
            that->update_leader();
        }

        if (ESP_OK != err || bytes_received < 1) {
            continue;
        }

        ESP_LOGD(TAG, "Got message from RMT");
        that->route(reinterpret_cast<uint8_t *>(buffer), bytes_received);
    }
}

int CommunicationRouter::send_msg(char* buffer, const size_t length) const {
    ESP_LOGD(TAG, "Got message from application");
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
        if (max == m_module_id) {
            m_pc_connection->connect();
        } else if (this->m_leader == m_module_id) {
            m_pc_connection->disconnect();
        }
    }

    this->m_leader = max;

    if (this->m_leader == m_module_id) {
        this->m_discovery_service->set_connected_boards(connected_module_ids);
    }
}

void CommunicationRouter::route(uint8_t* buffer, const size_t length) const {
	flatbuffers::Verifier verifier(buffer, length);
    // This could be moved to just be called on wireline data to save cpu cycles.
    if (bool ok = Messaging::VerifyMPIMessageBuffer(verifier); !ok) {
		ESP_LOGW(TAG, "route: got an invalid MPI message, disregarding");
		return;
	}

    if (const auto& mpi_message = Flatbuffers::MPIMessageBuilder::parse_mpi_message(buffer); mpi_message->destination() == m_module_id) {
        ESP_LOGD(TAG, "Routing to this module [dest: %d, length: %d]", static_cast<int>(mpi_message->destination()), length);
        this->m_rx_callback(reinterpret_cast<char *>(buffer), 512);
    } else if (mpi_message->destination() == PC_ADDR && this->m_leader == m_module_id) {
        ESP_LOGD(TAG, "Routing to wifi [dest: %d, length: %d]", static_cast<int>(mpi_message->destination()), length);
        this->m_lossless_server->send_msg(reinterpret_cast<char *>(buffer), 512);
    } else if (mpi_message->destination() == PC_ADDR) {
        ESP_LOGD(TAG, "Routing to wireline for wifi [dest: %d, length: %d]", static_cast<int>(mpi_message->destination()), length);
        this->m_data_link_manager->send(this->m_leader, buffer, length, FrameType::MOTOR_TYPE, 0);
    }else {
        ESP_LOGD(TAG, "Routing to wireline [dest: %d, length: %d]", static_cast<int>(mpi_message->destination()), length);
        this->m_data_link_manager->send(mpi_message->destination(), buffer, length, FrameType::MOTOR_TYPE, 0);
    }
}

std::pair<std::vector<uint8_t>, std::vector<Orientation>> CommunicationRouter::get_physically_connected_modules() const {
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

    return { connected_module_ids, connected_module_orientations };
}

[[nodiscard]] uint8_t CommunicationRouter::get_leader() const {
    return this->m_leader;
}
