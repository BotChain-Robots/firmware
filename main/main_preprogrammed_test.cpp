#if (defined(RMT_TEST) && RMT_TEST == 0) && (!defined(PP_MOVE) || (defined(PP_MOVE) && PP_MOVE == 1))
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

#if !defined(SRC_BOARD)
const uint8_t BOARD_ID = 1;
#else
const uint8_t BOARD_ID = SRC_BOARD
#endif

extern "C" [[noreturn]] void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    esp_event_loop_create_default();

    printf("finished esp init\n");

    printf("Hello world!\n");

    TopologyManager obj = TopologyManager();
    
    while(true){
        //do nothing
        vTaskDelay(1000 /    portTICK_PERIOD_MS);
    }
}
#endif