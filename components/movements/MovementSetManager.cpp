#include "MovementSetManager.h"
#include "esp_log.h"
#ifdef MOVEMENTS

MovementSetManager::MovementSetManager(){
    esp_err_t res = nvs_open(MOVEMENTS_NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (res != ESP_OK){
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to open nvs for %s", MOVEMENTS_NVS_NAMESPACE);
        return;
    }

    ready = true;
};

MovementSetManager::~MovementSetManager(){
    nvs_close(handle);
}

/**
 * @brief Adds a movement entry into the movements map
 * 
 * @param entry Movement entry information
 * @param index Sequence number of the movements
 * @return esp_err_t 
 */
esp_err_t MovementSetManager::add_movement(MovementEntryData& entry, uint8_t index){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    if (entry.board_id == PC_ADDR || entry.board_id == BROADCAST_ADDR){
        return ESP_ERR_INVALID_ARG;
    }

    if (movements.find(index) != movements.end()){
        for (MovementEntryData& e : movements[index]){
            if (e.board_id == entry.board_id){
                return ESP_ERR_INVALID_ARG; //only unique board ids per index
            }
        }
    }

    if (entry.post_delay_ms == 0){
        //we don't want 0ms delay in between the movements, so we will add the default delay
        entry.post_delay_ms = DEFAULT_DELAY_MS;
    }

    if (entry.ack != static_cast<uint8_t>(ACK_VALUES::NO_ACK) && entry.ack_ttl_ms == 0){
        entry.ack_ttl_ms = DEFAULT_ACK_TTL_MS;
    }
    
    movements[index].push_back(entry);

    return ESP_OK;
}

/**
 * @brief Removes a movement entry from the sequence, given the board id
 * 
 * @param index Sequence number
 * @param board_id 
 * @return esp_err_t 
 */
esp_err_t MovementSetManager::remove_movement(uint8_t index, uint8_t board_id){
    if (board_id == PC_ADDR || board_id == BROADCAST_ADDR){
        return ESP_ERR_INVALID_ARG;
    }

    if (movements.find(index) == movements.end()){
        return ESP_ERR_INVALID_ARG;
    }

    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    bool found = false;

    for (auto it = movements[index].begin(); it != movements[index].end(); ++it) {
        if (it->board_id == board_id){
            movements[index].erase(it);
            found = true;
            break;
        }
    }

    if (!found){
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
}

esp_err_t MovementSetManager::get_curr_movement_set(std::unordered_map<uint8_t, std::vector<MovementEntryData>>& set){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }
    
    set = movements;
    return ESP_OK;
}

esp_err_t MovementSetManager::get_nvs_movement_set(std::unordered_map<uint8_t, std::vector<MovementEntryData>>& set){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    size_t size = 0;
    esp_err_t res = nvs_get_u32(handle, MOVEMENTS_NVS_SET_SIZE_KEY, reinterpret_cast<uint32_t*>(&size));

    if (res != ESP_OK) {
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to read data size");
        return res;
    }

    std::vector<uint8_t> buffer(size);

    res = nvs_get_blob(handle, MOVEMENTS_NVS_SET_KEY, buffer.data(), &size);

    if (res != ESP_OK) {
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to read blob");
        return res;
    }

    // Verify the FlatBuffer
    flatbuffers::Verifier verifier(buffer.data(), buffer.size());
    if (!Movement::VerifyMovementSetBuffer(verifier)) {
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "FlatBuffer verification failed");
        return ESP_ERR_INVALID_ARG;
    }

    // Parse root
    const Movement::MovementSet* movement_set = Movement::GetMovementSet(buffer.data());

    movements.clear();

    const auto* map = movement_set->movement_map();
    if (!map) {
        return ESP_OK; // empty set
    }

    // Iterate map entries
    for (const Movement::MovementVector* mv : *map) {
        uint8_t key = mv->key();

        auto& vec = movements[key];
        vec.clear();

        const auto* entries = mv->movements();
        if (!entries) {
            continue;
        }

        vec.reserve(entries->size());

        // Must iterate over Movement::MovementEntry* (FlatBuffers table)
        for (const Movement::MovementEntry* e : *entries) {
            vec.push_back({
                e->board_id(),
                e->module_type(),
                e->value_action(),
                *e->condition(),   // copy struct
                e->ack(),
                e->ack_ttl_ms(),
                e->post_delay_ms()
            });
        }
    }

    set = movements;
    
    return ESP_OK;
}

esp_err_t MovementSetManager::write_nvs_movement_set(){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    Flatbuffers::SerializedMessage m = builder.build_movement_set(movements);

    esp_err_t res = nvs_set_u32(handle, MOVEMENTS_NVS_SET_SIZE_KEY, static_cast<uint32_t>(m.size));

    res = nvs_commit(handle);

    if (res != ESP_OK) {
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to commit data size");
        return res;
    }

    res = nvs_set_blob(handle, MOVEMENTS_NVS_SET_KEY, m.data, m.size);

    if (res != ESP_OK){
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to write to nvs");
        return res;
    }

    res = nvs_commit(handle);
    if (res != ESP_OK) {
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to commit topology");
        return res;
    }
    
    return ESP_OK;
}

#endif //MOVEMENTS