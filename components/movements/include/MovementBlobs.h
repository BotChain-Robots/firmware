#include "MovementManager.h"
#include "nvs_flash.h"
#include "TopologyBlobs.h"
#include <memory>
#include <vector>

#ifdef MOVEMENTS
#pragma once

typedef struct _condition_blob {
    uint16_t value;
    uint8_t cond;
    uint8_t moduleType;
    uint8_t in_use;
} ConditionBlob;

typedef struct _movement_entry {
    uint16_t board_id;
    uint8_t moduleType;
    uint16_t value_action;
    ConditionBlob condition;
    uint8_t ack;
    uint16_t ack_ttl_ms;
    uint16_t post_delay_ms;
} MovementEntry;

typedef struct _movement {
    uint8_t num_movements;
    MovementEntry movements[];
} MovementSet;

class MovementSetManager {
    public: 
        MovementSetManager();
        ~MovementSetManager();
        esp_err_t add_movement(MovementEntry& entry, uint8_t& index);
        esp_err_t remove_movement(uint8_t index);
        esp_err_t update_movement(MovementEntry& entry, uint8_t index);
        esp_err_t get_curr_movement_set(MovementSet& set);
        esp_err_t verify_movement_set();
        esp_err_t get_nvs_movement_set(MovementSet& set);
        esp_err_t write_nvs_movement_set();

    private:
        std::unique_ptr<TopologyManager> topology_manager;
        std::vector<MovementEntry> movements;
        nvs_handle_t handle;
};

#endif //MOVEMENTS