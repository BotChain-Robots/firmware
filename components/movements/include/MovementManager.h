#ifndef MOVEMENTS
#define MOVEMENTS

#include "nvs_flash.h"

#define MOVEMENTS_NVS_NAMESPACE "movements"
#define MOVEMENTS_NVS_KEY "key"
#define MOVEMENTS_NVS_TOPOLOGY_KEY "topology"
#define MOVEMENTS_DEBUG_TAG "movements"

class MovementManager {
    public:
        MovementManager();
        ~MovementManager();
        esp_err_t get_movement_status();

    private:
        nvs_handle_t handle = 0;
        esp_err_t status = ESP_ERR_INVALID_STATE;

};

#endif //MOVEMENTS