#if (defined(PP_MOVE) && PP_MOVE == 1) && (!defined(RMT_TEST) || (defined(RMT_TEST) && RMT_TEST == 0))
#include <cstdio>
#include <memory>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_flash.h"
#include "nvs_flash.h"

#include "constants/datalink.h"
#include "TopologyManager.h"

#include <esp_netif.h>
#include <esp_event.h>
#include <freertos/semphr.h>
#include "driver/gptimer.h"
#include "esp_log.h"
#include <vector>

#if !defined(SRC_BOARD)
const uint8_t BOARD_ID = 1;
#else
const uint8_t BOARD_ID = SRC_BOARD;
#endif
#define MOVEMENT_DEBUG_TAG "movement"

[[noreturn]] void restart(){
    for (int i = 5; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 /    portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}


extern "C" [[noreturn]] void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_event_loop_create_default();

    printf("finished esp init\n");

    printf("Hello world!\n");

    TopologyManager obj = TopologyManager();

    /**
     * @brief test topology
     * 
     * Board | Channel | Connected to
     * 1     | 0       | 2
     * 1     | 1       | 100
     * 2     | 0       | 1
     * 2     | 1       | 4
     * 2     | 2       | 7
     * 2     | 3       | 69
     * 4     | 2       | 2
     * 4     | 3       | 100
     * 7     | 0       | 69
     * 7     | 1       | 2
     * 69    | 0       | 2
     * 69    | 1       | 7
     * 100   | 0       | 4
     * 100   | 1       | 1
     */

    uint8_t succ_count = 0;

    std::vector<std::pair<std::vector<std::pair<uint8_t, uint16_t>>, uint16_t>> test_connections = {
        { { {0, 2}, {1, 100} }, 1 },
        { { {0, 1}, {1, 4}, {2, 7}, {3, 69} }, 2 },
        { { {2, 2}, {3, 100} }, 4 },
        { { {0, 69}, {1, 2} }, 7 },
        { { {0, 2}, {1, 7} }, 69 },
        { { {0, 4}, {1, 1} }, 100 }
    };

    for (const auto& connections : test_connections){
        ESP_LOGI(MOVEMENT_DEBUG_TAG, "Adding board %d...", connections.second);
        ESP_LOGI(MOVEMENT_DEBUG_TAG, "Connections:");
        
        for (const auto& c : connections.first){
            ESP_LOGI(MOVEMENT_DEBUG_TAG, "curr board %d -> board %d on channel %d", connections.second, c.second, c.first);
        }

        if (obj.add_board_to_topology(connections.first, connections.second) == ESP_OK) succ_count++;
        else{
            ESP_LOGE(MOVEMENT_DEBUG_TAG, "Failed to add board %d", connections.second);
        }
    }
    
    ESP_LOGI(MOVEMENT_DEBUG_TAG, "%d board connections successfully created out of %d", succ_count, test_connections.size());

    if (succ_count != test_connections.size()){
        restart();
    }

    succ_count = 0;

    esp_err_t res;

    for (const auto& connections : test_connections){
        std::vector<std::pair<uint8_t, uint16_t>> check;
        res = obj.get_board_in_topology(check, connections.second);

        if (res != ESP_OK){
            continue;
        }

        if (check.size() != connections.first.size()){
            continue;
        }

        bool broke = false;
        for (uint8_t i = 0; i < check.size(); i++){
            if (check.at(i) != connections.first.at(i)){
                broke = true;
                ESP_LOGE(MOVEMENT_DEBUG_TAG, "Board %d in topology is not correct! Failed at connection %d", connections.second, i);
                break;
            }
        }

        if (!broke){
            succ_count++;
        }
    }

    ESP_LOGI(MOVEMENT_DEBUG_TAG, "%d board connections successfully verified out of %d", succ_count, test_connections.size());

    res = obj.verify_topology();

    if (res != ESP_OK){
        ESP_LOGE(MOVEMENT_DEBUG_TAG, "Topology verification incorrectly failed");
        restart();
    } else {
        ESP_LOGI(MOVEMENT_DEBUG_TAG, "Topology verification passed");
    }

    res = obj.write_nvs_topology();
    
    if (res == ESP_OK){
        ESP_LOGI(MOVEMENT_DEBUG_TAG, "Topology NVS write success");
    } else {
        ESP_LOGE(MOVEMENT_DEBUG_TAG, "Topology NVS write failed");
        restart();
    }

    std::unordered_map<uint16_t, std::vector<std::pair<uint8_t, uint16_t>>> nvs_topology;

    res = obj.get_nvs_topology(nvs_topology);
    if (res == ESP_OK){
        ESP_LOGI(MOVEMENT_DEBUG_TAG, "Topology NVS read success");
    } else {
        ESP_LOGE(MOVEMENT_DEBUG_TAG, "Topology NVS read failed");
        restart();
    }

    if (nvs_topology.size() != test_connections.size()){
        ESP_LOGE(MOVEMENT_DEBUG_TAG, "Topology NVS size is different");
        restart();
    }

    for (const auto& connections : test_connections){
        const auto& test = connections.first;

        if (nvs_topology.find(connections.second) == nvs_topology.end()){
            ESP_LOGE(MOVEMENT_DEBUG_TAG, "Failed to find board %d in topology", connections.second);
            restart();
        }

        if (test.size() != nvs_topology[connections.second].size()){
            ESP_LOGE(MOVEMENT_DEBUG_TAG, "Board %d connections are different in topology", connections.second);
            restart();
        }
    }

    ESP_LOGI(MOVEMENT_DEBUG_TAG, "Topology NVS R/W success! Got back the same topology");

    ESP_LOGI(MOVEMENT_DEBUG_TAG, "Topology NVS got:");

    for (const auto& [key, connections] : nvs_topology){
        for (const auto& c : connections){
            ESP_LOGI(MOVEMENT_DEBUG_TAG, "Board %d -> board %d on channel %d", key, c.second, c.first);
        }
    }

    //test removing a board from topology

    uint16_t board_id_to_remove = 2;

    if (obj.remove_board_from_topology(board_id_to_remove) != ESP_OK){
        ESP_LOGE(MOVEMENT_DEBUG_TAG, "Failed to remove board %d", board_id_to_remove);
        restart();
    }

    ESP_LOGI(MOVEMENT_DEBUG_TAG, "Successfully removed board %d", board_id_to_remove);

    res = obj.verify_topology();

    if (res == ESP_OK){
        ESP_LOGE(MOVEMENT_DEBUG_TAG, "Topology verification incorrectly passed");
    } else {
        ESP_LOGI(MOVEMENT_DEBUG_TAG, "Topology verification correctly failed");
    }

    std::unordered_map<uint16_t, std::vector<std::pair<uint8_t, uint16_t>>> test_topology;

    if (obj.get_curr_topology(test_topology) != ESP_OK){
        ESP_LOGE(MOVEMENT_DEBUG_TAG, "Getting current topology failed");
        restart();
    }

    for (const auto& connections : test_connections){
        if (connections.second == board_id_to_remove){
            continue;
        }

        const auto& test = connections.first;

        if (test_topology.find(connections.second) == test_topology.end()){
            ESP_LOGE(MOVEMENT_DEBUG_TAG, "Failed to find board %d in topology", connections.second);
            restart();
        }

        if (test.size() != test_topology[connections.second].size()){
            ESP_LOGE(MOVEMENT_DEBUG_TAG, "Board %d connections are different in topology", connections.second);
            restart();
        }
    }

    ESP_LOGI(MOVEMENT_DEBUG_TAG, "Current topology is correct");

    restart();

    while(true){
        //do nothing
        vTaskDelay(1000 /    portTICK_PERIOD_MS);
    }
}
#endif