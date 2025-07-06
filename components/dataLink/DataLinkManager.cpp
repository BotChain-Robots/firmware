#include "DataLinkManager.h"
#include "RMTManager.h"
#include "esp_log.h"
#include "nvs_flash.h"

/**
 * @brief Construct a new Data Link Manager:: Data Link Manager object
 * 
 * @param board_id Board ID of the current board. Will be written to the NVM under key "board" if not already written.
 */
DataLinkManager::DataLinkManager(uint8_t board_id){
    //init table for this board and set up link layer priority queue
    phys_comms = std::make_unique<RMTManager>();
    if (phys_comms == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "RMT object was not created. Link layer communications will not function.");
        return;
    }

    if (get_board_id(this_board_id) != ESP_OK){
        //failed to read from NVM for board id under key "board". Will write a new entry
        this_board_id = board_id;
        set_board_id(this_board_id);
    }

    if (this_board_id != board_id){
        //NVM board id is different from `board_id` -> update entry to the new board id
        this_board_id = board_id;
        set_board_id(this_board_id);
    }

    init_rip();
}

DataLinkManager::~DataLinkManager(){
    phys_comms.reset(); //not strictly necessary to do this explicitly
}

esp_err_t DataLinkManager::set_board_id(uint8_t board_id){
    if (board_id == BROADCAST_ADDR || board_id == PC_ADDR){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid board id");
        return ESP_FAIL;
    }

    
    nvs_handle_t handle;
    esp_err_t res = nvs_open("board", NVS_READWRITE, &handle);
    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to open NVS Handle");
        return res;
    }
    
    res = nvs_set_u8(handle, "id", board_id);
    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to write ID %d to NVM", board_id);
        nvs_close(handle);
        return res;
    }
    
    res = nvs_commit(handle);
    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to commit write");
        nvs_close(handle);
        return res;
    }
    
    this_board_id = board_id;
    printf("Successfully wrote %d to NVM\n", board_id);

    nvs_close(handle);

    return ESP_OK;
}

esp_err_t DataLinkManager::get_board_id(uint8_t& board_id){
    nvs_handle_t handle;
    esp_err_t res = nvs_open("board", NVS_READWRITE, &handle);
    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to open NVS Handle");
        return res;
    }

    res = nvs_get_u8(handle, "id", &board_id);
    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to get ID from NVM. Please make sure NVM is already assigned a board id!");
        nvs_close(handle);
        return res;
    }

    printf("Successfully got board id %d from NVM\n", board_id);

    nvs_close(handle);
    
    return ESP_OK;
}

/**
 * @brief Sends a frame to another board (node to node communication) via RMT (physical layer)
 * 
 * @param dest_board 8 bit ID of the destination board
 * @param data 
 * @param data_len Length of the data in bytes
 * @param type 
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::send(uint8_t dest_board, uint8_t* data, uint16_t data_len, FrameType type, uint8_t curr_channel){
    if (phys_comms == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to send frame due to no RMT object");
        return ESP_FAIL;
    }

    if (data == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Data array does not exist");
        return ESP_FAIL;
    }

    if (this_board_id == PC_ADDR){
        ESP_LOGE(DEBUG_LINK_TAG, "This board is not assigned a board id");
        return ESP_FAIL;
    }
    
    if (curr_channel >= MAX_CHANNELS){
        return ESP_FAIL;
    }

    if (IS_CONTROL_FRAME(static_cast<uint8_t>(type))){
        //control frame
        if (data_len > MAX_CONTROL_DATA_LEN){
            ESP_LOGE(DEBUG_LINK_TAG, "Data for control frame is too large. Maximum size is %d. Current data length is %d", MAX_CONTROL_DATA_LEN, data_len);
            return ESP_FAIL;
        }

        control_frame new_frame = {
            .preamble = START_OF_FRAME,
            .sender_id = this_board_id,
            .receiver_id = dest_board,
            .seq_num = sequence_num_map[dest_board]++,
            .type_flag = static_cast<uint8_t>(type),
            .data_len = static_cast<uint8_t>(data_len),
            .crc_16 = 0, //not made yet
        };

        // printf("size of control frame %d\n", sizeof(control_frame));
        // printf("size of message %d\n", new_frame.data_len);
        // printf("message %s\n", data);
        // print_buffer_binary(data, new_frame.data_len);
        
        size_t frame_size = sizeof(control_frame) + new_frame.data_len - MAX_CONTROL_DATA_LEN;

        // printf("frame size %d\n", frame_size);

        uint8_t send_data[frame_size];
        size_t offset = 0;
        send_data[offset++] = new_frame.preamble;
        send_data[offset++] = new_frame.sender_id;
        send_data[offset++] = new_frame.receiver_id;
        send_data[offset++] = new_frame.seq_num & 0xFF;
        send_data[offset++] = (new_frame.seq_num >> 8) & 0xFF;
        send_data[offset++] = new_frame.type_flag;
        send_data[offset++] = new_frame.data_len;

        memcpy(&send_data[offset], data, new_frame.data_len);

        offset += new_frame.data_len;

        geneate_crc_16(send_data, offset, &new_frame.crc_16);

        send_data[offset++] = new_frame.crc_16 & 0xFF;
        send_data[offset++] = (new_frame.crc_16 >> 8) & 0xFF;


        rmt_transmit_config_t config = {
            .loop_count = 0,
            .flags = {
                .eot_level = 0   // typically 0 or 1, depending on your output idle level
            }
        };

        // printf("sending message:\n");
        // print_buffer_binary(send_data, frame_size);

        phys_comms->send(send_data, offset, &config, curr_channel);

        //can wait for the rmt to finish
        // esp_err_t res = phys_comms->wait_until_send_complete(curr_channel); //this cannot be here in deployment but until the RMT manager can hold this copy of data this will have to be here
    
        // if (res != ESP_OK){
        //     ESP_LOGE(DEBUG_LINK_TAG, "Failed to send message");
        //     return ESP_FAIL;
        // } else{
        //     // printf("Sent message to board %d\n", dest_board);
        // }

    } else {
        //generic frame
        printf("not implemented yet\n");
    }

    return ESP_OK;
}

void DataLinkManager::print_binary(uint8_t byte) {
    for (int i = 7; i >= 0; --i) {
        printf("%d", (byte >> i) & 1);
    }
}

void DataLinkManager::print_buffer_binary(const uint8_t* buffer, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        print_binary(buffer[i]);
        printf(" ");
    }
    printf("\n");
}

/**
 * @brief Starts the RMT async receive job to start listening for a new frame over a given channel
 * 
 * @param curr_channel 
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::start_receive_frames(uint8_t curr_channel){
    if (curr_channel >= MAX_CHANNELS){
        return ESP_FAIL;
    }
    return phys_comms->start_receiving(curr_channel);
}

esp_err_t DataLinkManager::receive(uint8_t* data, size_t data_len, size_t* recv_len, uint8_t curr_channel){
    if (data == NULL){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid data array");
        return ESP_FAIL;
    }
    
    if (curr_channel >= MAX_CHANNELS){
        return ESP_FAIL;
    }

    if (data_len < MAX_CONTROL_DATA_LEN + CONTROL_FRAME_OVERHEAD){
        return ESP_FAIL;
    }
    
    // uint8_t recv_buf[256];

    esp_err_t res = phys_comms->receive(data, data_len, recv_len, curr_channel);

    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "RMT Failed to receive");
        return ESP_FAIL;
    }
    
    if (*recv_len > MAX_CONTROL_DATA_LEN + CONTROL_FRAME_OVERHEAD){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid control frame");
        return ESP_FAIL;
    }

    //check for a rip frame
    if (static_cast<FrameType>((data[5])) == FrameType::RIP_TABLE_CONTROL){
        printf("Got a RIP frame\n");
        uint8_t rip_message[rip_table_valid_rows*2] = {};
        size_t rip_message_size = 0;
        
        res = get_data_from_frame(data, *recv_len, rip_message, &rip_message_size);

        if (res != ESP_OK){
            return ESP_FAIL; //crc or data len failed
        }

        for (size_t i = 0; i < rip_message_size-1; i+=2){
            uint8_t board_id = rip_message[i];
            uint8_t hops = rip_message[i+1];
            printf("Received: board_id %d and number of hops %d on channel %d\n", board_id, hops, curr_channel);

            RIPRow* entry = nullptr;

            res = rip_find_entry(board_id, &entry);
            if (res != ESP_OK){
                return ESP_FAIL;
            }

            if (entry == nullptr){
                return ESP_FAIL; //no room for more entries in the table
            }

            if (entry->valid == RIP_INVALID_ROW){
                //adding a new entry
                rip_add_entry(board_id, hops + 1, curr_channel, &entry);
            } else {
                //updating an entry
                rip_update_entry(hops + 1, curr_channel, &entry);
            }
        }
        *recv_len = 0;
    }

    return ESP_OK;
}

esp_err_t DataLinkManager::get_data_from_frame(uint8_t* data, size_t data_len, uint8_t* message, size_t* message_size){
    if (data == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid data array");
        return ESP_FAIL;
    }
    if (message == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid message array");
        return ESP_FAIL;
    }
    if (message_size == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid message size ptr");
        return ESP_FAIL;
    }

    if (IS_CONTROL_FRAME(data[5])){
        control_frame temp = {0};
    
        temp.preamble = data[0];
        temp.sender_id = data[1];
        temp.receiver_id = data[2];
        temp.seq_num = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
        temp.type_flag = data[5];
        temp.data_len = data[6];
    
        if (temp.data_len > data_len){
            return ESP_FAIL;
        }

        *message_size = temp.data_len;

        memcpy(temp.data, &data[7], temp.data_len);
        memcpy(message, &data[7], temp.data_len);
    
        geneate_crc_16(data, 7*sizeof(uint8_t) + temp.data_len, &temp.crc_16);
    
        if (((uint16_t)data[7 + temp.data_len] | ((uint16_t)data[8 + temp.data_len] << 8)) != temp.crc_16){
            //CRC mismatch
            ESP_LOGE(DEBUG_LINK_TAG, "CRC Mismatch");
            return ESP_FAIL;
        }
    
        // printf("Frame Information:\n");
        // printf("%-10s %-12s %-13s %-15s %-12s %-10s %-6s\n", 
        //    "Preamble", "Sender ID", "Receiver ID", "Sequence Num", "Type+Flag", "Data Len", "CRC");
    
        // printf("0x%02X       %-12d %-13d %-15d  0x%02X       %-10d   0x%04X\n", 
        //    temp.preamble, temp.sender_id, temp.receiver_id, temp.seq_num, temp.type_flag, temp.data_len, temp.crc_16);
    } else {
        //not implemented yet
    }


    return ESP_OK;
}

/**
 * @brief This function implements the CRC-16/CCITT algorithm
 * 
 * @param data 
 * @param data_len 
 * @param crc 
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::geneate_crc_16(uint8_t* data, size_t data_len, uint16_t* crc){
    if (data == nullptr){
        return ESP_FAIL;
    }

    if (data_len == 0){
        return ESP_FAIL; //fail if the data len is 0
    }

    *crc = 0x0;
    
    for (size_t i = 0; i < data_len; i++){
        uint8_t tbl_idx = (*crc >> 8) ^ data[i];
        *crc = (*crc << 8) ^ crc16_table[tbl_idx];
    }
    
    return ESP_OK;
}

esp_err_t DataLinkManager::print_frame_info(uint8_t* data, size_t data_len, uint8_t* message){
    printf("Received frame of size %d:\n", data_len);

    size_t message_size;

    // print_buffer_binary(data, data_len);
    return get_data_from_frame(data, data_len, message, &message_size);
}

/**
 * @brief Initializes the RIP table
 * 
 */
void DataLinkManager::init_rip(){
    for (size_t i = 0; i < RIP_MAX_ROUTES; i++){
        rip_table[i] = {
            .info = {
                .board_id = BROADCAST_ADDR, //invalid addr
                .hops = RIP_MAX_HOPS + 1, //infinite
            },
            .channel = MAX_CHANNELS + 1, //invalid channels
            .ttl = 0,
            .valid = RIP_INVALID_ROW,
            .row_sem = NULL
        };

        rip_table[i].row_sem = xSemaphoreCreateMutexStatic(&rip_table[i].mutex_buf);
    }

    //add the self route to the table
    rip_table[0].info = {
        .board_id = this_board_id,
        .hops = 0,
    };
    rip_table[0].channel = MAX_CHANNELS + 1;
    rip_table[0].ttl = RIP_TTL_START;
    rip_table[0].valid = 1;

    rip_table_valid_rows++;

    //temp debug
    rip_table[1].info = {
        .board_id = 69,
        .hops = 69,
    };
    rip_table[1].channel = MAX_CHANNELS + 1;
    rip_table[1].ttl = RIP_TTL_START;
    rip_table[1].valid = 1;
    rip_table_valid_rows++;

    rip_table[2].info = {
            .board_id = 3,
            .hops = 2,
        };
    rip_table[2].channel = MAX_CHANNELS + 1,
    rip_table[2].ttl = RIP_TTL_START,
    rip_table[2].valid = 1;
    rip_table_valid_rows++;

    start_rip_tasks();
}

esp_err_t DataLinkManager::rip_add_entry(uint8_t board_id, uint8_t hops, uint8_t channel, RIPRow** entry){
    if (entry == nullptr){
        return ESP_FAIL;
    }

    if (xSemaphoreTake((*entry)->row_sem, (TickType_t)RIP_MAX_SEM_WAIT) != pdTRUE){
        return ESP_FAIL;
    }

    (*entry)->channel = channel;
    (*entry)->info = {
        .board_id = board_id,
        .hops = hops
    };
    (*entry)->ttl = RIP_TTL_START;
    (*entry)->valid = 1;

    ESP_LOGI(DEBUG_LINK_TAG, "board_id %d now has hops %d from channel %d", (*entry)->info.board_id, (*entry)->info.hops, channel);

    xSemaphoreGive((*entry)->row_sem);

    return ESP_OK;
}

esp_err_t DataLinkManager::rip_reset_entry_ttl(uint8_t board_id){
    RIPRow* entry = nullptr;

    esp_err_t res;

    res = rip_find_entry(board_id, &entry);
    if (res != ESP_OK){
        return ESP_FAIL;
    }

    if (entry == nullptr){
        return ESP_FAIL; //board doesn't exist
    }

    if (xSemaphoreTake(entry->row_sem, (TickType_t)RIP_MAX_SEM_WAIT) != pdTRUE){
        return ESP_FAIL;
    }

    entry->ttl = RIP_TTL_START;

    xSemaphoreGive(entry->row_sem);

    return ESP_OK;
}

esp_err_t DataLinkManager::rip_update_entry(uint8_t new_hop, uint8_t channel, RIPRow** entry){
    if (entry == nullptr){
        return ESP_FAIL; //board doesn't exist
    }

    if (xSemaphoreTake((*entry)->row_sem, (TickType_t)RIP_MAX_SEM_WAIT) != pdTRUE){
        return ESP_FAIL;
    }

    if (new_hop < (*entry)->info.hops){
        (*entry)->info.hops = new_hop;
        (*entry)->channel = channel;
    }
    (*entry)->ttl = RIP_TTL_START;
    (*entry)->valid = 1;

    ESP_LOGI(DEBUG_LINK_TAG, "board_id %d now has hops %d from channel %d", (*entry)->info.board_id, (*entry)->info.hops, channel);

    xSemaphoreGive((*entry)->row_sem);

    return ESP_OK;
}

/**
 * @brief Finds the board_id in the table. If board_id does not exist in the table, `entry` will contain an empty row to write into.
 * TODO: use an unordered map instead of an array?
 * 
 * @param board_id 
 * @param entry 
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::rip_find_entry(uint8_t board_id, RIPRow** entry){
    for (size_t i = 0; i < RIP_MAX_ROUTES; i++){
        if (xSemaphoreTake(rip_table[i].row_sem, (TickType_t)RIP_MAX_SEM_WAIT) != pdTRUE){
            return ESP_FAIL;
        }
        if (rip_table[i].valid != RIP_INVALID_ROW && rip_table[i].info.board_id == board_id){
            *entry = &rip_table[i];
            xSemaphoreGive(rip_table[i].row_sem);
            break;
        } 
        if (rip_table[i].valid == RIP_INVALID_ROW){
            *entry = &rip_table[i];
        }
        xSemaphoreGive(rip_table[i].row_sem);
    }

    return ESP_OK;
}

esp_err_t DataLinkManager::broadcast_rip_frame(){
    //use the control frame for the demo (as the number of rows increase, we will need to use the generic frame)
    //data will be [board_id (1), hops (1), board_id (2), hops (2), ...]

    uint8_t rip_message[rip_table_valid_rows*2] = {};
    size_t message_idx = 0;

    for (size_t i = 0; i < RIP_MAX_ROUTES; i++){
        xSemaphoreTake(rip_table[i].row_sem, (TickType_t)RIP_MAX_SEM_WAIT);
        if (rip_table[i].valid == RIP_INVALID_ROW){
            xSemaphoreGive(rip_table[i].row_sem);
            continue;
        }

        rip_message[message_idx++] = rip_table[i].info.board_id;
        rip_message[message_idx++] = rip_table[i].info.hops;
        xSemaphoreGive(rip_table[i].row_sem);
    }

    esp_err_t res;

    for (uint8_t channel = 0; channel < MAX_CHANNELS; channel++){
        ESP_LOGI(DEBUG_LINK_TAG, "sending type %x",static_cast<uint8_t>(FrameType::RIP_TABLE_CONTROL));
        res = send(BROADCAST_ADDR, rip_message, message_idx, FrameType::RIP_TABLE_CONTROL, channel);
        if (res != ESP_OK){
            ESP_LOGE(DEBUG_LINK_TAG, "Failed to send rip frame on channel %d", channel);
        }
    }

    return ESP_OK;
}

[[noreturn]] void DataLinkManager::rip_broadcast_timer_function(void* args){
    DataLinkManager* link_layer_obj = static_cast<DataLinkManager*>(args);
    if (link_layer_obj == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "RIP Broadacst task failed to start due to invalid pointer");
        vTaskDelete(nullptr);
    }

    ESP_LOGI(DEBUG_LINK_TAG, "Starting RIP broadcast task");

    esp_err_t res;
    while(true){
        vTaskDelay(pdMS_TO_TICKS(RIP_BROADCAST_INTERVAL)); //wait RIP_BROADCAST_INTERVAL ms
        ESP_LOGI(DEBUG_LINK_TAG, "Broadcasting table..."); //debug
        res = link_layer_obj->broadcast_rip_frame();
        if (res != ESP_OK){
            ESP_LOGE(DEBUG_LINK_TAG, "Failed to broadcast rip frame");
        }
    }
}

[[noreturn]] void DataLinkManager::rip_ttl_decrement_task(void* args){
    DataLinkManager* link_layer_obj = static_cast<DataLinkManager*>(args);
    if (link_layer_obj == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "RIP Broadacst task failed to start due to invalid pointer");
        vTaskDelete(nullptr);
    }
    ESP_LOGI(DEBUG_LINK_TAG, "Starting RIP ttl decrement task");

    while(true){
        vTaskDelay(pdMS_TO_TICKS(RIP_MS_TO_SEC)); //run every second
        for (size_t i = 0; i < RIP_MAX_ROUTES; i++){
            ESP_LOGI(DEBUG_LINK_TAG, "Decrementing ttl on entry %d", i);
            if (xSemaphoreTake(link_layer_obj->rip_table[i].row_sem, (TickType_t)RIP_MAX_SEM_WAIT) !=pdTRUE){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to get sem from entry %d", i);
                continue;
            }
            if (link_layer_obj->rip_table[i].valid == RIP_INVALID_ROW){
                xSemaphoreGive(link_layer_obj->rip_table[i].row_sem);
                continue;
            }
            if (link_layer_obj->rip_table[i].ttl == 0){
                link_layer_obj->rip_table[i].valid = RIP_INVALID_ROW;
            } else {
                link_layer_obj->rip_table[i].ttl--;
            }
            ESP_LOGI(DEBUG_LINK_TAG, "Entry %d now has ttl %d", i, link_layer_obj->rip_table[i].ttl);
            xSemaphoreGive(link_layer_obj->rip_table[i].row_sem);
        }
    }
}

/**
 * @brief This function will start the tasks required for RIP to function.
 * Currently, this function will:
 * - start the task to periodically broadcast the board's current copy of the RIP table to all other boards via the 4 RMT channels
 * - start a task to periodically decrement the ttl values of each row in the RIP table (WIP) - this will require some sort of mutex on the table itself
 */
void DataLinkManager::start_rip_tasks(){
    ESP_LOGI(DEBUG_LINK_TAG, "Starting RIP Broadcast task");
    xTaskCreate(DataLinkManager::rip_broadcast_timer_function, "RIPBroadcastTask", 4096, static_cast<void*>(this), 5, NULL);
    ESP_LOGI(DEBUG_LINK_TAG, "Starting RIP TTL task");
    xTaskCreate(DataLinkManager::rip_ttl_decrement_task, "RIPTTLTask", 4096, static_cast<void*>(this), 5, NULL);
}