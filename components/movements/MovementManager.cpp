#include "MovementManager.h"
#include "nvs_flash.h"
#include "esp_log.h"
#ifdef MOVEMENTS

MovementManager::MovementManager(){
    esp_err_t res = nvs_open(MOVEMENTS_NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (res != ESP_OK){
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to open nvs for %s", MOVEMENTS_NVS_NAMESPACE);
        return;
    }

    status = ESP_OK;
};

esp_err_t MovementManager::get_movement_status(){
    return status;
}

MovementManager::~MovementManager(){
    nvs_close(handle);
}

#endif //MOVEMENTS