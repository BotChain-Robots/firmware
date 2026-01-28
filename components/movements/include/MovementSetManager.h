#include "MovementManager.h"
#include "nvs_flash.h"
#include "TopologyManager.h"
#include <memory>
#include <vector>
#include "MovementSetBuilder.h"
#include "unordered_map"

#ifdef MOVEMENTS
#pragma once

class MovementSetManager {
    public: 
        MovementSetManager();
        ~MovementSetManager();
        esp_err_t add_movement(Movement::MovementEntry& entry, uint8_t& index);
        esp_err_t remove_movement(uint8_t index, uint8_t board_id);
        esp_err_t update_movement(Movement::MovementEntry& entry, uint8_t index);
        esp_err_t get_curr_movement_set(Movement::MovementSet& set);
        esp_err_t verify_movement_set();
        esp_err_t get_nvs_movement_set(Movement::MovementSet& set);
        esp_err_t write_nvs_movement_set();

    private:
        std::unique_ptr<TopologyManager> topology_manager = std::make_unique<TopologyManager>();
        std::unordered_map<uint8_t, std::vector<Movement::MovementEntry>> movements;
        nvs_handle_t handle;
        bool ready = false;
        Flatbuffers::MovementSetBuilder builder;
};

#endif //MOVEMENTS