#if defined(RMT_TEST) && RMT_TEST == 1
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

#define DATA_SIZE_TEST 600

//current board id
#ifdef SRC_BOARD
#define BOARD_ID SRC_BOARD
#else 
#define BOARD_ID 1
#endif

//board id to send to
#ifdef RECEIVER_BOARD
#define RECEIVER_BOARD_ID RECEIVER_BOARD
#else
#define RECEIVER_BOARD_ID 69
#endif

struct TaskArgs{
    DataLinkManager* link_layer_obj;
    uint8_t task_id;
    uint8_t receiver_id;
    QueueHandle_t receive_queue;
};

struct ReceviedFrame{
    uint8_t buf[DATA_SIZE_TEST + GENERIC_FRAME_OVERHEAD];
    size_t len;
    FrameHeader header;
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

    ReceviedFrame recv_frame = {};

    FrameHeader header = {};

    while(true){          
        res = obj->async_receive(recv_buf, sizeof(recv_buf), &header, curr_channel);
        vTaskDelay(pdMS_TO_TICKS(10));
        if (res != ESP_OK){
            // ESP_LOGE("thread", "Failed to receive message on thread %d", curr_channel);
            if (res != ESP_ERR_NOT_FOUND) {
                recv_frame.len = 0;
                if (xQueueSendToBack(shared_queue, (void*)&recv_frame, (TickType_t) 10) != pdPASS){
                    ESP_LOGE("RX Job", "Failed to push received frame onto shared queue for channel %d", curr_channel);
                }
            } 
            continue;
        } else {
            // printf("Successfully receive message\n");
        }

        if (header.data_len == 0){
            continue;
        }

        recv_frame.len = header.data_len;
        memcpy((void*)recv_frame.buf, (void*)recv_buf, header.data_len);
        recv_frame.header = header;
        
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
    
    const char* message = "this is some other data that should not be corrupted.";
    const char* generic_frame_additional_message = "This is some other extra data that can be sent that also shouldn't be corrupted while transmitting.";
    const char* generic_frame_second_additional_message = "At this point we have reached 241 bytes. This is some other data that we can send using generic frames but this will be fragmented. however, this data shouldn't be corrupted and be sent as if it was sent all at once. total message size right here is 469 bytes:)";
    
    uint8_t curr_channel = args->task_id;
    QueueHandle_t shared_queue = (QueueHandle_t)args->receive_queue;
    
    uint8_t send_buf[DATA_SIZE_TEST];
    uint8_t recv_buf[DATA_SIZE_TEST];
    memset(recv_buf, 0, DATA_SIZE_TEST);
    memset(send_buf, 0, DATA_SIZE_TEST);
    
    size_t recv_len = 0;
    uint8_t iteration = 0;
    esp_err_t res = ESP_OK;
    
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
    #if defined(RECEIVE_ONLY) && RECEIVE_ONLY
    receive_only = true;
    #else
    receive_only = false;
    #endif

    while(1){
        if(!receive_only){
            printf("waiting for 3 seconds...\n");
            vTaskDelay(pdMS_TO_TICKS(3000)); // wait 3 second before trying to send again
    
            snprintf(reinterpret_cast<char*>(send_buf), sizeof(send_buf), "This is a message from board %d sending on channel %d. %s %s %s", BOARD_ID, curr_channel, message, generic_frame_additional_message, generic_frame_second_additional_message);
            // snprintf(reinterpret_cast<char*>(send_buf), sizeof(send_buf), "This is a message from board %d sending on channel %d. %s %s", BOARD_ID, curr_channel, message, generic_frame_additional_message);
            // snprintf(reinterpret_cast<char*>(send_buf), sizeof(send_buf), "This is a message from board %d sending on channel %d. %s", BOARD_ID, curr_channel, message);
            
            ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &start_count));
            res = obj->send(dest_board_id, send_buf, strlen(reinterpret_cast<char*>(send_buf)), FrameType::MISC_GENERIC_TYPE, 0x0);
            ESP_ERROR_CHECK(gptimer_get_raw_count(gptimer, &end_count));
    
            // snprintf(reinterpret_cast<char*>(send_buf), sizeof(send_buf), "%s RANDOM", mej.kssage); //modifying the data while it transmits shouldn't affect the actual transmission here
            
            if (res != ESP_OK){
                ESP_LOGE("thread", "Failed to send message on thread %d. Error: 0x%x", curr_channel, res);
                continue;
            } else {
                // printf("Successfully sent message %s\n", send_buf);
                printf("Sent %zu B sized in %" PRIu64 " us from channel %d\n", strlen(reinterpret_cast<char*>(send_buf)) + CONTROL_FRAME_OVERHEAD, end_count-start_count, curr_channel);
            }
        }
        
        //wait on a queue for a few ms (if there's nothing, just send another frame. otherwise pop from queue and read it)
        printf("waiting for rx\n");
        if (xQueueReceive(shared_queue, (void*)&recv_frame, pdMS_TO_TICKS(36000)) != pdPASS){
            memset(send_buf, 0, DATA_SIZE_TEST);
            printf("didn't receive anything in time\n");
            continue; //nothing or failed to pop from queue
        }
        
        total_transactions++;
        if (recv_frame.len == 0){
            //receive fail
            num_incorrect++;
        } else {
            // res = obj->print_frame_info(recv_frame.buf, recv_frame.len, recv_buf, DATA_SIZE_TEST);
            
            if (res != ESP_OK){
                num_incorrect++;
                // printf("Received %ld bad frames on tx/rx round %ld for thread %d\n", num_incorrect, total_transactions, curr_channel);
            } else {
                // printf("Header information:\n");
                // printf("Preamble\tTX ID\tRX ID\tSeq Num\tFrag Info\tType/Flag\tData Len\tCRC\n");
                // printf("0x%02X\t\t%-12d %-13d\t%-15d\t0x%02lX\t%d\t%-10d\t0x%04X\n",recv_frame.header.preamble, recv_frame.header.sender_id, recv_frame.header.receiver_id, recv_frame.header.seq_num, 
                //      recv_frame.header.frag_info, recv_frame.header.type_flag, recv_frame.header.data_len, recv_frame.header.crc_16);
                printf("Received message '%.*s' on channel %d from board %d seq num %d\n", recv_frame.len, recv_frame.buf, curr_channel, recv_frame.header.sender_id, recv_frame.header.seq_num);
            }
        }

        // printf("Total received packets: %ld\tTotal packets corrupted: %ld\n", total_transactions, num_incorrect);
        
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
    uint8_t num_channels = 4;
    std::unique_ptr<DataLinkManager> obj = std::make_unique<DataLinkManager>(BOARD_ID, num_channels);

    if (obj->ready() != ESP_OK){
        for (int i = 5; i >= 0; i--) {
            printf("Restarting in %d seconds...\n", i);
            vTaskDelay(1000 /    portTICK_PERIOD_MS);
        }
        printf("Restarting now.\n");
        fflush(stdout);
        esp_restart();
    }
    
    // uint8_t dest_board_id = 2; //using a dummy number for now - there is no board with id 2 right now
    
    // esp_err_t res;
    
    // uint8_t send_buf[256];
    // uint8_t recv_buf[256];
    // size_t recv_len = 0;
    
    // uint8_t curr_channel = 0;

    DataLinkManager* obj_to_send = obj.release();
    
    TaskArgs args[4] = {};

    for (uint8_t i = 0; i < 1; i++){
        args[i].link_layer_obj = obj_to_send;
        args[i].task_id = i;
        args[i].receiver_id = RECEIVER_BOARD_ID;
        args[i].receive_queue = xQueueCreate(10, sizeof(ReceviedFrame)); //queue storing up to 10 control frames
        xTaskCreate(multi_transceiver, "multi_transceiver", 8192, static_cast<void*>(&args[i]), 5, NULL);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    printf("Tasks have been created\n");

    while(true){
        //do nothing
        vTaskDelay(1000 /    portTICK_PERIOD_MS);
    }
}
#endif //RMT_TEST