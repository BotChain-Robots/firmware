#include "MovementSetManager.h"
#include "esp_log.h"
#ifdef MOVEMENTS

MovementSetManager::MovementSetManager(){
    esp_err_t res = nvs_open(MOVEMENTS_NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (res != ESP_OK){
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to open nvs for %s", MOVEMENTS_NVS_NAMESPACE);
        return;
    }
};

MovementSetManager::~MovementSetManager(){
    nvs_close(handle);
}

// esp_err_t MovementSetManager::add_movement(struct MovementEntry& entry, uint8_t& index){
//     return ESP_OK;
// }

// esp_err_t MovementSetManager::remove_movement(uint8_t index){
//     return ESP_OK;
// }

// esp_err_t MovementSetManager::update_movement(struct MovementEntry& entry, uint8_t index){
//     return ESP_OK;
// }

// esp_err_t MovementSetManager::get_curr_movement_set(struct MovementSet& set){
//     return ESP_OK;
// }

// esp_err_t MovementSetManager::verify_movement_set(){
//     return ESP_OK;
// }

// esp_err_t MovementSetManager::get_nvs_movement_set(struct MovementSet& set){
//     return ESP_OK;
// }

// esp_err_t MovementSetManager::write_nvs_movement_set(){
//     return ESP_OK;
// }

#endif //MOVEMENTS