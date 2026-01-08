//
// Created by Johnathon Slightham on 2025-07-05.
//

#include <memory>

#include "LoopManager.h"
#include "SensorMessageBuilder.h"
#include "TopologyMessageBuilder.h"

#define ACTUATOR_CMD_TAG 5
#define TOPOLOGY_CMD_TAG 6
#define METADATA_RX_TAG 7
#define SENSOR_TAG 8

#define METADATA_PERIOD_MS 1000
#define SENSOR_DATA_PERIOD_MS 1000

[[noreturn]] void LoopManager::control_loop() const {
    uint8_t buffer[512];
    while (true) {
        m_messaging_interface->recv(reinterpret_cast<char *>(buffer), 512, PC_ADDR, ACTUATOR_CMD_TAG);
        m_actuator->actuate(buffer);
        send_sensor_reading(false);
    }
}


[[noreturn]] void LoopManager::sensor_loop(char * args) {
    const auto that = reinterpret_cast<LoopManager *>(args);

    while (true) {
        that->send_sensor_reading(true);
        vTaskDelay(SENSOR_DATA_PERIOD_MS / portTICK_PERIOD_MS);
    }
}

[[noreturn]] void LoopManager::metadata_tx_loop(char * args) {
    const auto that = reinterpret_cast<LoopManager *>(args);
    const auto topology_message_builder = std::make_unique<Flatbuffers::TopologyMessageBuilder>();
    while (true) {
        const auto [module_ids, orientations] =  that->m_messaging_interface->get_physically_connected_modules();
        // todo: this is awful, we can't cast from a vector of orientation to int.... :(
        std::vector<int8_t> casted_orientations{};
        casted_orientations.reserve(orientations.size());
        for (const auto orientation : orientations) {
            casted_orientations.emplace_back(orientation);
        }

        const auto [data, size] = topology_message_builder->build_topology_message(
            that->m_config_manager.get_module_id(),
            that->m_config_manager.get_module_type(),
            module_ids,
            casted_orientations,
            that->m_messaging_interface->get_connection_type(),
            that->m_messaging_interface->get_leader());
        that->m_messaging_interface->send(static_cast<char *>(data), size, PC_ADDR, TOPOLOGY_CMD_TAG, false);
        vTaskDelay(METADATA_PERIOD_MS / portTICK_PERIOD_MS);
    }
}

[[noreturn]] void LoopManager::metadata_rx_loop(char *args) {
    const auto that = reinterpret_cast<LoopManager *>(args);
    const auto buffer = std::make_unique<std::vector<char>>();
    buffer->resize(512);
    while (true) {
        that->m_messaging_interface->recv(buffer->data(), 512, PC_ADDR, METADATA_RX_TAG);
    }
}

void LoopManager::send_sensor_reading(bool durable) const {
    Flatbuffers::SensorMessageBuilder smb{};
    // todo: get data from sensor
    auto data = m_actuator->get_sensor_data();
    const auto [ptr, size] = smb.build_sensor_message(data);
    m_messaging_interface->send(reinterpret_cast<char *>(ptr), size, PC_ADDR, SENSOR_TAG, durable);
}
