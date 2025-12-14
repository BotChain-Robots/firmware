#include "unity.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" [[noreturn]] void app_main(void){
    ESP_ERROR_CHECK(nvs_flash_init());
    unity_run_all_tests();

    while(true){
        //do nothing
        vTaskDelay(1000 /    portTICK_PERIOD_MS);
    }
    // for (int i = 5; i >= 0; i--) {
    //     printf("Restarting in %d seconds...\n", i);
    //     vTaskDelay(1000 /    portTICK_PERIOD_MS);
    // }
    // printf("Restarting now.\n");
    // fflush(stdout);
    // esp_restart();
}