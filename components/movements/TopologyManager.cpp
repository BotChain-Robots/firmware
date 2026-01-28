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

bool TopologyManager::is_ready(){
    return ready;
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
esp_err_t TopologyManager::add_board_to_topology(const std::vector<std::pair<uint8_t, uint16_t>>& connections, uint16_t curr_board_id){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    if (connections.size() == 0 || connections.size() > MAX_CHANNELS){
        ESP_LOGE(TOPOLOGY_DEBUG_TAG, "vector size is invalid. got size %d", connections.size());
        return ESP_ERR_INVALID_ARG;
    }

    if (curr_board_id == PC_ADDR || curr_board_id == BROADCAST_ADDR){
        ESP_LOGE(TOPOLOGY_DEBUG_TAG, "curr_board_id is invalid. got size %d", curr_board_id);
        return ESP_ERR_INVALID_ARG;
    }

    //verify the connections in the vector
    std::unordered_set<uint8_t> check_set;

    std::vector<Topology::ChannelBoardConn> conns;

    for (const std::pair<uint8_t, uint16_t>& pair : connections){
        if (pair.first >= MAX_CHANNELS || pair.second == curr_board_id || pair.second == PC_ADDR 
            || pair.second == BROADCAST_ADDR || check_set.find(pair.first) != check_set.end()){
            ESP_LOGE(TOPOLOGY_DEBUG_TAG, "Invalid pair detected. Got: %d -> %d on Ch. %d", curr_board_id, pair.second, pair.first);
            return ESP_ERR_INVALID_ARG;
        }

        check_set.insert(pair.first);

        conns.push_back(builder.build_connections(pair.first, pair.second));
    }    

    topology[curr_board_id] = conns;
    
    return ESP_OK;
}

/**
 * @brief Removes the board associated with `board_id`
 * 
 * @warning This function does not remove any existing links on other boards on the topology
 * 
 * @param board_id 
 * @return esp_err_t 
 */
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

/**
 * @brief Returns a vector of connections that represents the channel connection to a board id from `curr_board_id`
 * 
 * @param connections 
 * @param curr_board_id 
 * @return esp_err_t 
 */
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

/**
 * @brief Verifies the topology
 * 
 * Conditions to fail:
 * 
 * 1. Any board that is referenced in the topology does not have a reciprocal connection (eg. A -> B and B -> A)
 * 
 * 2. The topology contains 2 separate graphs (a board in the topology should be in the same graph with some sort of path to all other boards)
 * 
 * @return esp_err_t 
 */
esp_err_t TopologyManager::verify_topology(){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    if (topology.size() == 0 || topology.size() >= RIP_MAX_ROUTES){
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
                ESP_LOGE(TOPOLOGY_DEBUG_TAG, "Could not find board %d in topology", conn.board_id());
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
                ESP_LOGE(TOPOLOGY_DEBUG_TAG, "Board %d does not have a connection to board %d", conn.board_id(), pair.first);
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

        // printf("On board %d\n", curr_node);

        if (visited.find(curr_node) != visited.end()) {
            continue;
        }

        visited.insert(curr_node);
        count++;

        const std::vector<Topology::ChannelBoardConn>& conns = topology[curr_node];

        // printf("Board %d has %d connections\n", curr_node, conns.size());

        for (const Topology::ChannelBoardConn& conn : conns) {
            uint16_t next = conn.board_id();
            if (visited.find(next) == visited.end()) {
                // printf("Found new board %d\n", next);
                backtrack.push(next);
            }
        }
    }

    if (count != topology.size()){
        ESP_LOGE(TOPOLOGY_DEBUG_TAG, "Not all boards could be traversed to all other boards. Got %d out of total %d boards", count, topology.size());
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

/**
 * @brief Gets the current topology stored in the manager
 * 
 * @param topology 
 * @return esp_err_t 
 */
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

/**
 * @brief Writes the topology stored in the manager onto the board's NVS
 * 
 * @return esp_err_t 
 */
esp_err_t TopologyManager::write_nvs_topology(){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    Flatbuffers::SerializedMessage m = builder.build_topology(topology);

    ESP_LOGI(TOPOLOGY_DEBUG_TAG, "Saving topology blob...");

    esp_err_t res = nvs_set_u32(handle, MOVEMENTS_NVS_TOPOLOGY_DATA_SIZE_KEY, static_cast<uint32_t>(m.size));

    res = nvs_commit(handle);
    if (res != ESP_OK) {
        ESP_LOGE(TOPOLOGY_DEBUG_TAG, "Failed to commit data size");
        return res;
    }

    res = nvs_set_blob(handle, MOVEMENTS_NVS_TOPOLOGY_KEY, m.data, m.size);

    if (res != ESP_OK){
        ESP_LOGE(TOPOLOGY_DEBUG_TAG, "Failed to write to nvs");
        return res;
    }

    res = nvs_commit(handle);
    if (res != ESP_OK) {
        ESP_LOGE(TOPOLOGY_DEBUG_TAG, "Failed to commit topology");
        return res;
    }

    return ESP_OK;
}

/**
 * @brief Read's the board NVS to retreve the saved topology (if it exists)
 * 
 * @param topology 
 * @return esp_err_t 
 */
esp_err_t TopologyManager::get_nvs_topology(std::unordered_map<uint16_t, std::vector<std::pair<uint8_t, uint16_t>>>& topology){
    if (!ready) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t size = 0;
    esp_err_t res = nvs_get_u32(handle,MOVEMENTS_NVS_TOPOLOGY_DATA_SIZE_KEY, reinterpret_cast<uint32_t*>(&size));

    if (res != ESP_OK) {
        ESP_LOGE(TOPOLOGY_DEBUG_TAG, "Failed to read data size");
        return res;
    }

    std::vector<uint8_t> buffer(size);

    res = nvs_get_blob(handle, MOVEMENTS_NVS_TOPOLOGY_KEY, buffer.data(), &size);

    if (res != ESP_OK) {
        ESP_LOGE(TOPOLOGY_DEBUG_TAG, "Failed to read blob");
        return res;
    }

    // Verify the FlatBuffer
    flatbuffers::Verifier verifier(buffer.data(), buffer.size());
    if (!Topology::VerifyTopologyInfoBuffer(verifier)) {
        ESP_LOGE(TOPOLOGY_DEBUG_TAG, "FlatBuffer verification failed");
        return ESP_ERR_INVALID_ARG;
    }

    // Parse root
    const Topology::TopologyInfo* topo = Topology::GetTopologyInfo(buffer.data());

    topology.clear();

    // Replace neighbours() with your actual accessor name
    auto neighbours = topo->boards();

    for (const auto* nb : *neighbours) {
        uint16_t board_id = nb->curr_board_id();

        std::vector<std::pair<uint8_t, uint16_t>> connections;
        connections.reserve(nb->neighbour_connections()->size());

        for (const auto* conn : *nb->neighbour_connections()) {
            connections.emplace_back(conn->channel(), conn->board_id());
        }

        topology.emplace(board_id, std::move(connections));
    }

    return ESP_OK;
}

#endif //MOVEMENTS