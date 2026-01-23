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

/**
 * @brief Creates a `Topology::NeighbourBlob` based on the input parameters and adds the blob to the internal topology map
 * 
 * @param connections This is a vector of pairs (uint8_t: `channel number`, uint16_t: `board_id`) representing the connection from the `curr_board_id` to `board_id` via `channel number`
 * @param curr_board_id This is the board id, from its perspective, has the connections to the other boards in the vector `connections`
 * @return esp_err_t 
 * 
 * @note
 * Connections to fail:
 * 
 * 1. Any of the `board_id` in the vector is `PC_ADDR` or `BROADCAST_ADDR` - Reserved addresses
 * 
 * 2. Any of the `board_id` in the vector is `curr_board_id` - No loopback connections are allowed
 * 
 * 3. Any of the `channel` in the vector is greater than or equal to `MAX_CHANNELS` - Channels are 0-indexed and there are at most 4 channels
 * 
 * 4. All `board_id` is not unique in the vector - Should be a 1:1 connection between the boards
 */
esp_err_t TopologyManager::add_board_to_topology(std::vector<std::pair<uint8_t, uint16_t>>& connections, uint16_t curr_board_id){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    if (connections.size() == 0 || connections.size() > MAX_CHANNELS){
        return ESP_ERR_INVALID_ARG;
    }

    if (curr_board_id == PC_ADDR || curr_board_id == BROADCAST_ADDR){
        return ESP_ERR_INVALID_ARG;
    }

    //verify the connections in the vector
    std::unordered_set<uint8_t> check_set;

    std::vector<Topology::ChannelBoardConn> conns;

    for (const std::pair<uint8_t, uint16_t>& pair : connections){
        if (pair.second >= MAX_CHANNELS || pair.first == curr_board_id || pair.first == PC_ADDR 
            || pair.first == BROADCAST_ADDR || check_set.find(pair.first) != check_set.end()){
            return ESP_ERR_INVALID_ARG;
        }

        check_set.insert(pair.first);

        conns.push_back(builder.build_connections(pair.first, pair.second));
    }    

    topology[curr_board_id] = conns;
    
    return ESP_OK;
}

esp_err_t TopologyManager::remove_board_from_topology(uint16_t board_id){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    if (topology.find(board_id) == topology.end()){
        return ESP_ERR_NOT_FOUND;
    }

    topology.erase(board_id);

    return ESP_OK;
}

esp_err_t TopologyManager::get_board_in_topology(std::vector<std::pair<uint8_t, uint16_t>>& connections, uint16_t curr_board_id){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    if (topology.find(curr_board_id) == topology.end()){
        return ESP_ERR_NOT_FOUND;
    }

    std::vector<Topology::ChannelBoardConn> conn = topology[curr_board_id];

    connections.clear();

    for (const Topology::ChannelBoardConn c : conn){
        connections.push_back(std::pair<uint8_t, uint16_t>(c.channel(), c.board_id()));
    }

    return ESP_OK;
}

esp_err_t TopologyManager::verify_topology(){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    if (topology.size() == 0){
        return ESP_ERR_INVALID_STATE;
    }

    /**
     * Conditions to fail:
     * 1. Any board that is referenced in the topology does not have a reciprocal connection (eg. A -> B and B -> A)
     * 2. The topology contains 2 separate graphs (a board in the topology should be in the same graph with some sort of path to all other boards)
     */

    //condition 1

    for (const auto& pair : topology){
        for (const Topology::ChannelBoardConn& conn : pair.second){
            if (topology.find(conn.board_id()) == topology.end()){
                return ESP_ERR_INVALID_STATE; //could not find reciprocal board
            }

            std::vector<Topology::ChannelBoardConn> topology_board_conn = topology[conn.board_id()];
            bool found = false;
            for (const Topology::ChannelBoardConn& c : topology_board_conn){
                if (c.board_id() == pair.first){
                    found = true;
                    break;
                }
            }
            if (!found){
                return ESP_ERR_INVALID_STATE; //reciprocal board does not have a connection back to `pair.first`
            }

        }
    }

    //condition 2 - dfs
    std::unordered_set<uint16_t> visited;
    std::stack<uint16_t> backtrack;
    uint32_t count = 0;

    backtrack.push(topology.begin()->first);
    
    while (!backtrack.empty()){
        uint16_t curr_node = backtrack.top();
        backtrack.pop();

        if (visited.find(curr_node) != visited.end()) {
            continue;
        }

        visited.insert(curr_node);
        count++;

        const std::vector<Topology::ChannelBoardConn>& conns = topology[curr_node];

        for (const Topology::ChannelBoardConn& conn : conns) {
            uint16_t next = conn.board_id();
            if (visited.find(next) != visited.end()) {
                backtrack.push(next);
            }
        }
    }

    if (count != topology.size()){
        return ESP_ERR_INVALID_STATE;
    }


    return ESP_OK;
}

esp_err_t TopologyManager::get_curr_topology(std::unordered_map<uint16_t, std::vector<std::pair<uint8_t, uint16_t>>>& topology){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    topology.clear();

    for (const auto& [key, value] : this->topology){
        topology[key] = std::vector<std::pair<uint8_t, uint16_t>>();
        for (const Topology::ChannelBoardConn c : value){
            topology[key].push_back(std::pair<uint8_t, uint16_t>(c.channel(), c.board_id()));
        }
    }

    return ESP_OK;
}

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