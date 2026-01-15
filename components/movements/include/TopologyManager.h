#include "MovementManager.h"
#include "nvs_flash.h"
#include <unordered_map>

// move these to constants header file?
#include "RMTManager.h"
#include "DataLinkManager.h"
#include "Frames.h"

#ifdef MOVEMENTS
#pragma once

class TopologyManager {
    public:
        TopologyManager();
        ~TopologyManager();
        // esp_err_t add_board_to_topology(struct NeighbourBlob& blob);
        // esp_err_t remove_board_from_topology(uint16_t board_id);
        // esp_err_t get_board_in_topology(struct NeighbourBlob& blob);
        // esp_err_t verify_topology();
        // esp_err_t get_curr_topology(struct Topology& topology);
        // esp_err_t write_nvs_topology();
        // esp_err_t get_nvs_topology(struct Topology& topology);

    private:
        // std::unordered_map<uint16_t, struct NeighbourBlob> topology;
        nvs_handle_t handle;
        bool ready = false;
};

#endif //MOVEMENTS