#include "MovementManager.h"
#include "nvs_flash.h"
#include "TopologyManager.h"
#include <memory>
#include <vector>

#ifdef MOVEMENTS
#pragma once

class MovementSetManager {
    public: 
        MovementSetManager();
        ~MovementSetManager();
        // esp_err_t add_movement(struct MovementEntry& entry, uint8_t& index);
        // esp_err_t remove_movement(uint8_t index);
        // esp_err_t update_movement(struct MovementEntry& entry, uint8_t index);
        // esp_err_t get_curr_movement_set(struct MovementSet& set);
        // esp_err_t verify_movement_set();
        // esp_err_t get_nvs_movement_set(struct MovementSet& set);
        // esp_err_t write_nvs_movement_set();

    private:
        std::unique_ptr<TopologyManager> topology_manager;
        std::vector<struct MovementEntry> movements;
        nvs_handle_t handle;
};

#endif //MOVEMENTS