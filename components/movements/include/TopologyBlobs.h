#include "MovementManager.h"
#include "nvs_flash.h"
#include <unordered_map>

#ifdef MOVEMENTS
#pragma once

typedef struct _channel_board_conn {
    uint8_t channel;
    uint16_t board_id;
} ChannelBoardConn;

typedef struct _neighbour_blob {
    uint16_t curr_board_id;
    uint8_t num_connections;  
    ChannelBoardConn neighbour_connections[];
} NeighbourBlob;

typedef struct _topology {
    uint16_t num_boards;
    NeighbourBlob boards[];
} Topology;

class TopologyManager {
    public:
        TopologyManager();
        ~TopologyManager();
        esp_err_t add_board_to_topology(NeighbourBlob blob);
        esp_err_t remove_board_from_topology(uint16_t board_id);
        esp_err_t update_board_in_topology(NeighbourBlob blob);
        esp_err_t get_board_in_topology(NeighbourBlob& blob);
        esp_err_t verify_topology();
        esp_err_t get_curr_topology(Topology& topology);
        esp_err_t write_nvs_topology();
        esp_err_t get_nvs_topology(Topology& topology);

    private:
        std::unordered_map<uint16_t, NeighbourBlob> topology;
        nvs_handle_t handle;
};

#endif //MOVEMENTS