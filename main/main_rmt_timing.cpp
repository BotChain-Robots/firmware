#include <cstdio>
#include <memory>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_flash.h"
#include "nvs_flash.h"

#include "RMTManager.h"

#include <esp_netif.h>
#include <esp_event.h>
#include <freertos/semphr.h>

#define BOARD_A_MESSAGE "MESSAGE FROM BOARD A"
#define BOARD_B_MESSAGE "MESSAGE FROM BOARD B"

#ifdef TIME_TEST
    #include <inttypes.h>
    #include "driver/gptimer.h"
#endif //TIME_TEST

// void rmt_task(void* arg) {
//     vTaskDelay(pdMS_TO_TICKS(3000)); // wait 3 seconds to stabilize heap
//     const auto obj = std::make_unique<RMTManager>();

//     const char* message = "THIS IS A SAMPLE TEXT MESSAGE";
//     rmt_transmit_config_t tx_config = {
//         .loop_count = 0,
//         .flags = {
//             .eot_level = 0   // typically 0 or 1, depending on your output idle level
//         }
//     };

//     int res = obj->send(message, strlen(message), &tx_config);

//     if (res == ESP_OK){
//         printf("Successfully sent '%s'\n", message);
//     } else{
//         printf("Failed to send '%s'\n", message);
//     }

//     vTaskDelete(NULL);
// }

void print_binary(unsigned char c) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (c >> i) & 1);
    }
}

void print_string_binary(const char *str) {
    while (*str) {
        print_binary((unsigned char)*str);
        printf(" "); // space between bytes for readability
        str++;
    }
    printf("\n");
}

/**
 * @brief This main function shows the RMT TX and RX working by sending a message string in `message` over a GPIO pin and receiving on another pin
 * 
 */
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

    const auto obj = std::make_unique<RMTManager>();
    
    #ifdef TIME_TEST
        gptimer_handle_t gptimer = NULL;
        gptimer_config_t timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = 1 * 1000 * 1000, // 1MHz, 1 tick = 1us
        };
        ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));
        ESP_ERROR_CHECK(gptimer_enable(gptimer));
        ESP_ERROR_CHECK(gptimer_start(gptimer));
        uint64_t start_count = 0, end_count = 0;
        uint64_t sum = 0; //used to calculate the average send time
        uint64_t num_iterations = 0;
    #endif //TIME_TEST
        
    #ifdef BOARD_A
        const char* message = BOARD_A_MESSAGE;
    #elif BOARD_B
        const char* message = BOARD_B_MESSAGE;
    #else
        const char* message = "THIS IS A SAMPLE TEXT MESSAGE";
    #endif

    #ifdef VERIFY_RECEIVE
        uint64_t num_received = 0;
        uint64_t num_corrupted = 0;
    #endif //VERIFY_RECEIVE

    // const char* message = "t";
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
        .flags = {
            .eot_level = 0   // typically 0 or 1, depending on your output idle level
        }
    };
    
    int res = ESP_OK;
    
    char recv_message[256];

    // xTaskCreate(rmt_task, "rmt_task", 4096, NULL, 5, NULL);
    while(true){
        #ifndef TIME_TEST
            printf("Starting RX receive\n");
            res = obj->start_receiving();
            if (res != ESP_OK){
                printf("Something went wrong... terminating..\n");
                continue;
            }
        #endif //TIME_TEST

        printf("sending message %s - binary:\n", message);
        print_string_binary(message);

        #ifdef TIME_TEST
            ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &start_count));
        #endif //TIME_TEST

        res = obj->send(message, strlen(message), &tx_config); 
        
        if (res == ESP_OK){
            // printf("Successfully started send job for message '%s'\n", message);
        } else{
            printf("Failed to start send job for message '%s'\n", message);
            // continue; //do not continue on
        }

        res = obj->wait_until_send_complete(); //will wait until the the message is sent

        #ifdef TIME_TEST
            ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &end_count));
        #endif //TIME_TEST

        if (res == ESP_OK){
            #ifndef TIME_TEST
            printf("Successfully sent message '%s'\n", message);
            #else
            printf("Sent %zu B sized message %s in %" PRIu64 " us on iteration %" PRIu64 "\n", strlen(message), message, end_count-start_count, num_iterations);
            sum += (end_count - start_count);
            #endif //TIME_TEST
        } else{
            printf("Failed to send '%s'\n", message);
            continue;
        }
        
        #ifndef TIME_TEST    
            res = obj->receive(recv_message, sizeof(recv_message));
            
            if (res != 0){
                printf("Failed to receive message\n");
            } else {
                printf("Received message %s\n", recv_message);
            }
            #ifdef VERIFY_RECEIVE
                printf("Checking message for corruption on iteration %lld\n", num_received);
                #ifdef BOARD_A
                    //check if BOARD_B_MESSAGE was received correctly
                    if (strcmp(recv_message, BOARD_B_MESSAGE) != 0){
                        num_corrupted++;
                    }
                #elif BOARD_B
                    if (strcmp(recv_message, BOARD_A_MESSAGE) != 0){
                        num_corrupted++;
                    }
                #endif //BOARD_B

                num_received++;

            #endif //VERIFY_RECEIVE

            memset(recv_message, 0, sizeof(recv_message));
        #endif //TIME_TEST
            
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        #ifdef TIME_TEST
        
            num_iterations++;
            if (num_iterations > 100){
                break;
            }
        #endif //TIME_TEST

        #ifdef VERIFY_RECEIVE
            if (num_received > 100){
                break;
            }
        #endif //VERIFY_RECEIVE
    }

    #ifdef TIME_TEST
        float avg = (sum/num_iterations) / 1e6; //avg send time us to s
        printf("Average Transmission Rate is: %.9f bits per second\n", (float)((strlen(message) * 8)/avg));
        printf("Average sent time is: %.9f seconds\n", avg);
    #endif //TIME_TEST

    #ifdef VERIFY_RECEIVE
        float avg_received_corrupted = (num_corrupted * 100) / (num_received-1);
        printf("Average corruption rate is: %.6f %% \n", avg_received_corrupted);
        printf("Total number of corrupted messages over %lld iterations is: %lld\n", num_received-1, num_corrupted);
    #endif //VERIFY_RECEIVE

    for (int i = 5; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 /    portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();

    while(true){
        //dummy wait
        vTaskDelay(2000 / portTICK_PERIOD_MS);

    }
}
