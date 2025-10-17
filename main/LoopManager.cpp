//
// Created by Johnathon Slightham on 2025-07-05.
//


#include <iostream>
#include <memory>

#include "LoopManager.h"
#include "MessagingInterface.h"
#include "TopologyMessageBuilder.h"
#include "control/ActuatorFactory.h"
#include "esp_log.h"

#define ACTUATOR_CMD_TAG 5
#define TOPOLOGY_CMD_TAG 6

#define METADATA_PERIOD_MS 1000

[[noreturn]] void LoopManager::control_loop() const {
    const auto actuator = ActuatorFactory::create_actuator(m_config_manager.get_module_type());

    uint8_t buffer[512];
    while (true) {
        m_messaging_interface->recv(reinterpret_cast<char *>(buffer), 512, PC_ADDR, ACTUATOR_CMD_TAG);
        actuator->actuate(buffer);
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
        that->m_messaging_interface->send(static_cast<char *>(data), size, PC_ADDR, TOPOLOGY_CMD_TAG, true);
        vTaskDelay(METADATA_PERIOD_MS / portTICK_PERIOD_MS);
    }
}
