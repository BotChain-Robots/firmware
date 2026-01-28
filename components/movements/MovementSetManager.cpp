#include "MovementSetManager.h"
#include "esp_log.h"
#ifdef MOVEMENTS

MovementSetManager::MovementSetManager(){
    esp_err_t res = nvs_open(MOVEMENTS_NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (res != ESP_OK){
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to open nvs for %s", MOVEMENTS_NVS_NAMESPACE);
        return;
    }

    if (!topology_manager->is_ready()){
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Topology Manager is not ready!");
        return;
    }

    ready = true;
};

MovementSetManager::~MovementSetManager(){
    nvs_close(handle);
}

esp_err_t MovementSetManager::add_movement(Movement::MovementEntry& entry, uint8_t& index){
    if (!ready){
        return ESP_ERR_INVALID_STATE;
    }

    if (entry.board_id() == PC_ADDR || entry.board_id() == BROADCAST_ADDR){
        return ESP_ERR_INVALID_ARG;
    }

    if (movements.find(index) != movements.end()){
        for (Movement::MovementEntry& e : movements[index]){
            if (e.board_id() == entry.board_id()){
                return ESP_ERR_INVALID_ARG; //only unique board ids per index
            }
        }
    }
    
    movements[index].push_back(entry);

    return ESP_OK;
}

esp_err_t MovementSetManager::remove_movement(uint8_t index, uint8_t board_id){
    if (board_id == PC_ADDR || board_id == BROADCAST_ADDR){
        return ESP_ERR_INVALID_ARG;
    }

    if (movements.find(index) == movements.end()){
        return ESP_ERR_INVALID_ARG;
    }

    bool found = false;

    for (auto it = movements[index].begin(); it != movements[index].end(); ++it) {
        if (it->board_id() == board_id){
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

esp_err_t MovementSetManager::update_movement(Movement::MovementEntry& entry, uint8_t index){
    return ESP_OK;
}

esp_err_t MovementSetManager::get_curr_movement_set(Movement::MovementSet& set){
    return ESP_OK;
}

esp_err_t MovementSetManager::verify_movement_set(){
    return ESP_OK;
}

esp_err_t MovementSetManager::get_nvs_movement_set(Movement::MovementSet& set){
    return ESP_OK;
}

esp_err_t MovementSetManager::write_nvs_movement_set(){
    return ESP_OK;
}

#endif //MOVEMENTS