//Used for link layer testing (change name to main.cpp to use)
#include <cstdio>
#include <memory>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_flash.h"
#include "nvs_flash.h"

#include "RMTManager.h"
#include "DataLinkManager.h"

#include <esp_netif.h>
#include <esp_event.h>
#include <freertos/semphr.h>
#include "driver/gptimer.h"
#include "esp_log.h"

#define DATA_SIZE_TEST 270

struct TaskArgs{
    DataLinkManager* link_layer_obj;
    uint8_t task_id;
    uint8_t receiver_id;
    QueueHandle_t receive_queue;
};

struct ReceviedFrame{
    uint8_t buf[MAX_CONTROL_DATA_LEN + CONTROL_FRAME_OVERHEAD]; //max 41B
    size_t len;
};

void receive_frames(void* arg){
    TaskArgs* args = (TaskArgs*)arg;
    
    DataLinkManager* obj = args->link_layer_obj;
    
    if (obj == nullptr){
        ESP_LOGE("thread", "bad pointer\n");
        vTaskDelete(NULL); //should never get here
    }

    QueueHandle_t shared_queue = (QueueHandle_t)args->receive_queue;

    uint8_t curr_channel = args-> task_id;

    printf("RX JOB for task %d starting...\n", curr_channel);
    esp_err_t res;

    uint8_t recv_buf[DATA_SIZE_TEST];
    memset(recv_buf, 0, DATA_SIZE_TEST);
    size_t recv_len = 0;

    ReceviedFrame recv_frame = {};

    while(true){
        res = obj->start_receive_frames(curr_channel); // this will be moved to a separate thread with a shared queue
        if (res != ESP_OK){
            ESP_LOGE("thread", "Failed to start rx async job on thread %d", curr_channel);
            continue;
        }

        res = obj->receive(recv_buf, sizeof(recv_buf), &recv_len, curr_channel);
        if (res != ESP_OK){
            ESP_LOGE("thread", "Failed to receive message on thread %d", curr_channel);
            continue;
        } else {
            // printf("Successfully receive message\n");
        }

        if (recv_len == 0){
            continue;
        }

        recv_frame.len = recv_len;
        memcpy((void*)recv_frame.buf, (void*)recv_buf, recv_len);
        
        if (xQueueSendToBack(shared_queue, (void*)&recv_frame, (TickType_t) 10) != pdPASS){
            ESP_LOGE("RX Job", "Failed to push received frame onto shared queue for channel %d", curr_channel);
        }
    }

}

void multi_transceiver(void* arg) {
    TaskArgs* args = (TaskArgs*)arg;

    DataLinkManager* obj = args->link_layer_obj;

    if (obj == nullptr){
        ESP_LOGE("thread", "bad pointer\n");
        vTaskDelete(NULL); //should never get here
    }

    xTaskCreate(receive_frames, "receive_frame_job", 4096, arg, 5, NULL);

    uint8_t dest_board_id = args->receiver_id; //using a dummy number for now - there is no board with id 2 right now
    
    const char* message = "FROM BEST BOARD";
    
    uint8_t curr_channel = args->task_id;
    QueueHandle_t shared_queue = (QueueHandle_t)args->receive_queue;
    
    uint8_t send_buf[DATA_SIZE_TEST];
    uint8_t recv_buf[DATA_SIZE_TEST];
    memset(recv_buf, 0, DATA_SIZE_TEST);
    memset(send_buf, 0, DATA_SIZE_TEST);
    
    size_t recv_len = 0;
    uint8_t iteration = 0;
    esp_err_t res;
    
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
    
    uint32_t num_incorrect = 0;
    uint32_t total_transactions = 0;

    RIPRow_public_matrix matrix[RIP_MAX_ROUTES];
    size_t matrix_size = RIP_MAX_ROUTES;

    for (int i = 0; i < RIP_MAX_ROUTES; i++){
        RIPRow_public* table = (RIPRow_public*)pvPortMalloc(sizeof(RIPRow_public)*RIP_MAX_ROUTES);
        matrix[i] = {
            .table = table,
            .size = RIP_MAX_ROUTES
        };
    }
    
    ReceviedFrame recv_frame = {};
    printf("task %d starting...\n", curr_channel);
    vTaskDelay(3000 /    portTICK_PERIOD_MS);

    bool receive_only = false;

    while(1){
        if(!receive_only){
            vTaskDelay(1000 /    portTICK_PERIOD_MS); // wait 1 second before trying to send again
    
            snprintf(reinterpret_cast<char*>(send_buf), sizeof(send_buf), "%s %d CH. %d", message, 4, curr_channel);
            
            // ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &start_count));
            res = obj->send(dest_board_id, send_buf, strlen(reinterpret_cast<char*>(send_buf)), FrameType::DEBUG_CONTROL_TYPE, 0x0);
            // ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &end_count));
    
            snprintf(reinterpret_cast<char*>(send_buf), sizeof(send_buf), "%s RANDOM", message); //modifying the data while it transmits shouldn't affect the actual transmission here
            
            if (res != ESP_OK){
                ESP_LOGE("thread", "Failed to send message on thread %d", curr_channel);
                continue;
            } else {
                printf("Successfully sent message\n");
                // printf("Sent %zu B sized in %" PRIu64 " us\n", strlen(reinterpret_cast<char*>(send_buf)) + CONTROL_FRAME_OVERHEAD, end_count-start_count);
            }
        }
        
        if (receive_only){
            //wait on a queue for a few ms (if there's nothing, just send another frame. otherwise pop from queue and read it)
            if (xQueueReceive(shared_queue, (void*)&recv_frame, (TickType_t) 50) != pdPASS){
                memset(send_buf, 0, 256);
                continue; //nothing or failed to pop from queue
            }
    
            res = obj->print_frame_info(recv_frame.buf, recv_frame.len, recv_buf);
            
            if (res != ESP_OK){
                num_incorrect++;
                printf("Received %ld bad frames on tx/rx round %ld for thread %d\n", num_incorrect, total_transactions, curr_channel);
            } else {
                printf("Received message %s on channel %d on round %ld. Total bad frames %ld\n", recv_buf, curr_channel, total_transactions, num_incorrect);
            }
        }
        
        total_transactions++;

        iteration++;
        if (iteration == 10){
            iteration = 0;
            // if (!receive_only){
            //     matrix_size = RIP_MAX_ROUTES;
            //     res = obj->get_network_toplogy(matrix, &matrix_size);
            //     if (res != ESP_OK){
            //         ESP_LOGE("multi", "Failed to get topology");
            //     } else {
            //         for (int i = 0; i < matrix_size; i++){
            //             printf("Table for board %d:\n", matrix[i].board_id);
            //             printf("board_id\t\tHops\t\tChannel\n");
            //             for (int j = 0; j < matrix[i].size; j++){
            //                 printf("%d\t\t%d\t\t%d\n", matrix[i].table[j].info.board_id, matrix[i].table[j].info.hops, matrix[i].table[j].channel);
            //             }
            //             printf("=====\n");

            //             //reset matrix
            //             matrix[i].size = RIP_MAX_ROUTES;
            //         }
            //     }
            // }
        }
        
        // vTaskDelay(1000 /    portTICK_PERIOD_MS); // wait 1 second before trying to send again
        //reset temp buffers
        memset(recv_buf, 0, DATA_SIZE_TEST);
        memset(send_buf, 0, DATA_SIZE_TEST);
        
    }

    while(true){
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

}

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
    
    // uint8_t iteration = 0;
    // const char* message = "THIS IS A TEXT MESSAGE";
    uint8_t num_channels = 1;
    uint8_t board_id = 4;
    std::unique_ptr<DataLinkManager> obj = std::make_unique<DataLinkManager>(board_id, num_channels);
    
    // uint8_t dest_board_id = 2; //using a dummy number for now - there is no board with id 2 right now
    
    // esp_err_t res;
    
    // uint8_t send_buf[256];
    // uint8_t recv_buf[256];
    // size_t recv_len = 0;
    
    // uint8_t curr_channel = 0;

    DataLinkManager* obj_to_send = obj.release();
    
    TaskArgs args[4] = {};

    for (uint8_t i = 0; i < num_channels; i++){
        args[i].link_layer_obj = obj_to_send;
        args[i].task_id = i;
        args[i].receiver_id = 69;
        args[i].receive_queue = xQueueCreate(10, sizeof(ReceviedFrame)); //queue storing up to 10 control frames
        xTaskCreate(multi_transceiver, "multi_transceiver", 4096, static_cast<void*>(&args[i]), 5, NULL);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    printf("Tasks have been created\n");

    while(true){
        //do nothing
        vTaskDelay(1000 /    portTICK_PERIOD_MS);
    }

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
