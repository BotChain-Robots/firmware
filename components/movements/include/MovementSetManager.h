#include "MovementManager.h"
#include "nvs_flash.h"
#include <memory>
#include <vector>
#include "MovementSetBuilder.h"
#include "unordered_map"
#include "constants/rmt.h"
#include "constants/datalink.h"

#ifdef MOVEMENTS
#pragma once

#define DEFAULT_DELAY_MS 50 //some random value that i am using to default to for the delay after a movement has been completed
#define DEFAULT_ACK_TTL_MS 100
#define CONDITION_BLOB_NOT_IN_USE 0
#define CONDITION_BLOB_IN_USE 1

enum class ACK_VALUES : uint8_t {
    NO_ACK, //no acks required
    ACK, //acks required to be sent and received properly within ttl
    ACK_NO_FAIL, //acks required to be send but not necessarily required to be received properly within ttl
};

enum class CONDITION_VALUES : uint8_t {
    LT,
    LEQ,
    GT,
    GEQ,
    EQ, 
    NE,
};

class MovementSetManager {
    public: 
        MovementSetManager();
        ~MovementSetManager();
        esp_err_t add_movement(MovementEntryData& entry, uint8_t index);
        esp_err_t remove_movement(uint8_t index, uint8_t board_id);
        esp_err_t get_curr_movement_set(std::unordered_map<uint8_t, std::vector<MovementEntryData>>& set);
        esp_err_t get_nvs_movement_set(std::unordered_map<uint8_t, std::vector<MovementEntryData>>& set);
        esp_err_t write_nvs_movement_set();

    private:
        std::unordered_map<uint8_t, std::vector<MovementEntryData>> movements;
        nvs_handle_t handle;
        bool ready = false;
        Flatbuffers::MovementSetBuilder builder;
};

#endif //MOVEMENTS