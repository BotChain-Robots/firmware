#include "TopologyManager.h"
#include "esp_log.h"
#ifdef MOVEMENTS

TopologyManager::TopologyManager(){
    esp_err_t res = nvs_open(MOVEMENTS_NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (res != ESP_OK){
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to open nvs for %s", MOVEMENTS_NVS_NAMESPACE);
        return;
    }

    ready = true;
};

TopologyManager::~TopologyManager(){
    nvs_close(handle);
}

// esp_err_t TopologyManager::add_board_to_topology(struct NeighbourBlob& blob){
//     if (!ready){
//         return ESP_ERR_INVALID_STATE;
//     }

//     if (blob.num_connections > MAX_CHANNELS || blob.curr_board_id == PC_ADDR || blob.curr_board_id == BROADCAST_ADDR){
//         return ESP_ERR_INVALID_ARG;
//     }

//     for (uint8_t i = 0; i < blob.num_connections; i++){
//         ChannelBoardConn connection = blob.neighbour_connections[i];
//         if (connection.channel > MAX_CHANNELS || connection.board_id == PC_ADDR 
//             || connection == BROADCAST_ADDR || connection.board_id == blob.curr_board_id){
//                 return ESP_ERR_INVALID_ARG;
//             }
//     }

//     topology[blob.curr_board_id] = blob;
    
//     return ESP_OK;
// }

// esp_err_t TopologyManager::remove_board_from_topology(uint16_t board_id){
//     if (!ready){
//         return ESP_ERR_INVALID_STATE;
//     }

//     if (topology.find(board_id) == topology.end()){
//         return ESP_ERR_NOT_FOUND;
//     }

//     topology.erase(board_id);

//     return ESP_OK;
// }

// esp_err_t TopologyManager::get_board_in_topology(struct NeighbourBlob& blob){
//     if (!ready){
//         return ESP_ERR_INVALID_STATE;
//     }

//     if (topology.find(board_id) == topology.end()){
//         return ESP_ERR_NOT_FOUND;
//     }

//     blob = topology.find(board_id);

//     return ESP_OK;
// }

// esp_err_t TopologyManager::verify_topology(){
//     if (!ready){
//         return ESP_ERR_INVALID_STATE;
//     }
//     return ESP_OK;
// }

// esp_err_t TopologyManager::get_curr_topology(struct Topology& topology){
//     if (!ready){
//         return ESP_ERR_INVALID_STATE;
//     }

//     return ESP_OK;
// }

// esp_err_t TopologyManager::write_nvs_topology(){
//     if (!ready){
//         return ESP_ERR_INVALID_STATE;
//     }
//     return ESP_OK;
// }

// esp_err_t TopologyManager::get_nvs_topology(struct Topology& topology){
//     if (!ready){
//         return ESP_ERR_INVALID_STATE;
//     }
//     return ESP_OK;
// }

#endif //MOVEMENTS