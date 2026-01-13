#include "TopologyBlobs.h"
#include "esp_log.h"
#ifdef MOVEMENTS

TopologyManager::TopologyManager(){
    esp_err_t res = nvs_open(MOVEMENTS_NVS_NAMESPACE, NVS_READWRITE, &handle);

    if (res != ESP_OK){
        ESP_LOGE(MOVEMENTS_DEBUG_TAG, "Failed to open nvs for %s", MOVEMENTS_NVS_NAMESPACE);
        return;
    }
};

TopologyManager::~TopologyManager(){
    nvs_close(handle);
}

#endif //MOVEMENTS