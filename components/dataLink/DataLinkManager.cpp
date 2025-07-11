#include "DataLinkManager.h"
#include "RMTManager.h"
#include "esp_log.h"
#include "nvs_flash.h"

/**
 * @brief Construct a new Data Link Manager:: Data Link Manager object
 * 
 * @param board_id Board ID of the current board. Will be written to the NVM under key "board" if not already written.
 */
DataLinkManager::DataLinkManager(uint8_t board_id, uint8_t num_channels = MAX_CHANNELS){
    //init table for this board and set up link layer priority queue
    phys_comms = std::make_unique<RMTManager>(num_channels);
    if (phys_comms == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "RMT object was not created. Link layer communications will not function.");
        return;
    }

    if (get_board_id(this_board_id) != ESP_OK){
        //failed to read from NVM for board id under key "board". Will write a new entry
        this_board_id = board_id;
        set_board_id(this_board_id);
    }

    // if (this_board_id != board_id){
    //     //NVM board id is different from `board_id` -> update entry to the new board id
    //     this_board_id = board_id;
    //     set_board_id(this_board_id);
    // }

    this->num_channels = num_channels;

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
esp_err_t DataLinkManager::send(uint8_t dest_board, uint8_t* data, uint16_t data_len, FrameType type, uint8_t flag){
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

    esp_err_t res;

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
            .type_flag = (uint8_t)((static_cast<uint8_t>(type) & 0xF0) | (flag & 0xF)),
            .data_len = static_cast<uint8_t>(data_len),
            .crc_16 = 0, //not made yet
        };

        ESP_LOGI(DEBUG_LINK_TAG, "type flag %X\n", new_frame.type_flag);
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
        send_data[offset++] = (new_frame.data_len >> 8) & 0xFF;

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

        uint8_t channel_to_route = MAX_CHANNELS;
        if (new_frame.receiver_id == BROADCAST_ADDR){
            for (uint8_t i = 0; i < num_channels; i++){
                phys_comms->send(send_data, offset, &config, i);
            }

        } else {
            res = route_frame(new_frame.receiver_id, &channel_to_route);
    
            if (res != ESP_OK){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to find entry for %d", new_frame.receiver_id);
                return ESP_FAIL;
            }
            phys_comms->send(send_data, offset, &config, channel_to_route);
        }

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
    if (curr_channel >= num_channels){
        return ESP_FAIL;
    }
    return phys_comms->start_receiving(curr_channel);
}

esp_err_t DataLinkManager::receive(uint8_t* data, size_t data_len, size_t* recv_len, uint8_t curr_channel){
    if (data == NULL){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid data array");
        return ESP_FAIL;
    }
    
    if (curr_channel >= num_channels){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid channel");
        return ESP_FAIL;
    }

    if (data_len < MAX_CONTROL_DATA_LEN + CONTROL_FRAME_OVERHEAD){
        ESP_LOGE(DEBUG_LINK_TAG, "Receive data buffer len is too small");
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

    uint8_t* message = (uint8_t*)pvPortMalloc(CONTROL_FRAME_OVERHEAD + MAX_CONTROL_DATA_LEN);
    if (message == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to malloc for receive");
        return ESP_FAIL;
    }
    memset(message, 0, sizeof(message));
    size_t message_size = 0;
    frame_header header;
    res = get_data_from_frame(data, *recv_len, message, &message_size, &header);
    if (res != ESP_OK){
        vPortFree((void*)message);
        return ESP_FAIL;
    }
    // ESP_LOGI(DEBUG_LINK_TAG, "Received frame of type 0x%X destined for board %d", GET_TYPE(header.type_flag), header.receiver_id);
    
    //check for a rip frame
    if (static_cast<FrameType>(GET_TYPE(header.type_flag)) == FrameType::RIP_TABLE_CONTROL){
        printf("Got a RIP frame\n");

        for (size_t i = 0; i < message_size-1; i+=2){
            uint8_t board_id = message[i];
            uint8_t hops = message[i+1];
            ESP_LOGI(DEBUG_LINK_TAG, "Received: board_id %d and number of hops %d on channel %d", board_id, hops, curr_channel);
            
            RIPRow* entry = nullptr;
            
            res = rip_find_entry(board_id, &entry, true);
            if (res != ESP_OK){
                vPortFree((void*)message);
                return ESP_FAIL;
            }
            
            if (entry == nullptr){
                printf("rip pointer\n");
                vPortFree((void*)message);
                return ESP_FAIL; //no room for more entries in the table
            }
            
            if (entry->valid == RIP_NEW_ROW){
                //adding a new entry
                rip_add_entry(board_id, hops + 1, curr_channel, &entry);
            } else {
                //updating an entry
                rip_update_entry(hops + 1, curr_channel, &entry);
            }
            
            if (GET_FLAG(header.type_flag) == FLAG_DISCOVERY){
                //discovery -> send routing table
                ESP_LOGI(DEBUG_LINK_TAG, "got discovery reply");
                RIPRow_public row_queue = {
                    .info = entry->info,
                    .channel = entry->channel
                };
                
                xQueueSendToBack(discovery_tables, &row_queue, (TickType_t)10);
            }
            
        }
        *recv_len = 0;
        if (message_size == RIP_DISCOVERY_MESSAGE_SIZE){
            res = send_rip_frame(false, header.sender_id);
            if (res != ESP_OK){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to send back rip table to board %d", header.sender_id);
                return res;
            }
        }
    }

    //got frame but not destined for this board
    if (header.receiver_id != this_board_id && header.receiver_id != BROADCAST_ADDR && header.seq_num > sequence_num_map[header.receiver_id]){
        ESP_LOGI(DEBUG_LINK_TAG, "Sending message to board %d with message %s", header.receiver_id, message);
        res = send(header.receiver_id, message, message_size, FrameType::DEBUG_CONTROL_TYPE, 0);
        *recv_len = 0;
        vPortFree((void*)message);
        return res;
    }

    vPortFree((void*)message);
    return ESP_OK;
}

esp_err_t DataLinkManager::get_data_from_frame(uint8_t* data, size_t data_len, uint8_t* message, size_t* message_size, frame_header* header){
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
    if (header == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid header ptr");
        return ESP_FAIL;
    }

    if (IS_CONTROL_FRAME(data[5])){
        header->preamble = data[0];
        header->sender_id = data[1];
        header->receiver_id = data[2];
        header->seq_num = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
        header->type_flag = data[5];
        header->data_len = (uint16_t)data[6] | ((uint16_t)data[7] << 8);
    
        if (header->data_len > data_len){
            ESP_LOGE(DEBUG_LINK_TAG, "Mismatch data length in control frame");
            return ESP_FAIL;
        }

        if (header->data_len == 0){
            ESP_LOGE(DEBUG_LINK_TAG, "Data len 0");
            return ESP_FAIL;
        }

        *message_size = header->data_len;

        memcpy(message, &data[8], header->data_len);
    
        geneate_crc_16(data, 8*sizeof(uint8_t) + header->data_len, &header->crc_16);
    
        if (((uint16_t)data[8 + header->data_len] | ((uint16_t)data[9 + header->data_len] << 8)) != header->crc_16){
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

    frame_header temp;

    // print_buffer_binary(data, data_len);
    return get_data_from_frame(data, data_len, message, &message_size, &temp);
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
            .ttl_flush = 0,
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

    //temp debug
    // rip_table[1].info = {
    //     .board_id = 2,
    //     .hops = 1,
    // };
    // rip_table[1].channel = 0;
    // rip_table[1].ttl = RIP_TTL_START;
    // rip_table[1].valid = 1;

    // rip_table[2].info = {
    //         .board_id = 1,
    //         .hops = 2,
    //     };
    // rip_table[2].channel = 0,
    // rip_table[2].ttl = RIP_TTL_START,
    // rip_table[2].valid = 1;

    discovery_tables = xQueueCreate(RIP_MAX_ROUTES, sizeof(RIPRow_public));

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
    
    if (uxQueueMessagesWaiting(manual_broadcasts) == 0){
        bool dummy = true;
        xQueueSend(manual_broadcasts, &dummy, 0); //new row - send broadcast
    }

    return ESP_OK;
}

esp_err_t DataLinkManager::rip_reset_entry_ttl(uint8_t board_id){
    RIPRow* entry = nullptr;

    esp_err_t res;

    res = rip_find_entry(board_id, &entry, false);
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

    uint8_t old_hops = (*entry)->info.hops;

    if ((*entry)->info.hops > new_hop && (*entry)->info.hops != RIP_MAX_HOPS + 1){ //no count to infinity if path is invalid
        (*entry)->info.hops = new_hop;
        (*entry)->channel = channel;
        ESP_LOGI(DEBUG_LINK_TAG, "updated board_id %d now has hops %d from channel %d", (*entry)->info.board_id, (*entry)->info.hops, channel);
    }
    (*entry)->ttl = RIP_TTL_START;
    (*entry)->valid = 1;

    ESP_LOGI(DEBUG_LINK_TAG, "refreshed board_id %d ttl", (*entry)->info.board_id);

    xSemaphoreGive((*entry)->row_sem);

    if (uxQueueMessagesWaiting(manual_broadcasts) == 0 && old_hops > new_hop && old_hops != RIP_MAX_HOPS + 1){
        //if hops were changed, send broadcast (if there isn't already one manual broadcast request pending)
        bool dummy = true;
        xQueueSend(manual_broadcasts, &dummy, 0);
    }

    return ESP_OK;
}

/**
 * @brief Finds the board_id in the table if it exists and stores that row in `entry`
 * TODO: use an unordered map instead of an array?
 * 
 * @param board_id 
 * @param entry 
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::rip_find_entry(uint8_t board_id, RIPRow** entry, bool reserve_row = false){
    RIPRow* free_slot = nullptr;
    for (size_t i = 0; i < RIP_MAX_ROUTES; i++){
        if (xSemaphoreTake(rip_table[i].row_sem, (TickType_t)RIP_MAX_SEM_WAIT) != pdTRUE){
            return ESP_FAIL;
        }
        if (rip_table[i].valid == RIP_VALID_ROW && rip_table[i].info.board_id == board_id){
            *entry = &rip_table[i];
            xSemaphoreGive(rip_table[i].row_sem);
            ESP_LOGI(DEBUG_LINK_TAG, "Found %d in table at row %d", board_id, i);
            return ESP_OK;
        } 
        if (rip_table[i].valid == RIP_INVALID_ROW && free_slot == nullptr){
            free_slot = &rip_table[i];
        }
        xSemaphoreGive(rip_table[i].row_sem);
    }

    ESP_LOGI(DEBUG_LINK_TAG, "Finished looking for %d in table", board_id);

    if (!reserve_row){
        return ESP_OK;
    }

    if (free_slot != nullptr){
        if (xSemaphoreTake(free_slot->row_sem, RIP_MAX_SEM_WAIT) != pdTRUE) {
            return ESP_FAIL;
        }

        // IMPORTANT: Mark it as taken so others don't grab it
        free_slot->valid = RIP_NEW_ROW; // Or some other init state
        free_slot->info.board_id = board_id;
        *entry = free_slot;

        xSemaphoreGive(free_slot->row_sem);
        ESP_LOGI(DEBUG_LINK_TAG, "Reserved new entry for board %d", board_id);
    }

    return ESP_OK;
}

/**
 * @brief Sends RIP frame
 * 
 * @param broadcast True - broadcasts (sends rip table to all available channels); False - sends rip table via routing based on `dest_id`
 * @param dest_id Destination board (requesting board) to send the rip table to (ignored if `broadcast is true`)
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::send_rip_frame(bool broadcast, uint8_t dest_id){
    //use the control frame for the demo (as the number of rows increase, we will need to use the generic frame)
    //data will be [board_id (1), hops (1), board_id (2), hops (2), ...]

    uint8_t rip_message[RIP_MAX_ROUTES*2] = {};
    size_t message_idx = 0;

    for (size_t i = 0; i < RIP_MAX_ROUTES; i++){
        xSemaphoreTake(rip_table[i].row_sem, (TickType_t)RIP_MAX_SEM_WAIT);
        if (rip_table[i].valid == RIP_INVALID_ROW){
            xSemaphoreGive(rip_table[i].row_sem);
            continue;
        }

        if (rip_table[i].info.hops == RIP_MAX_HOPS + 1){
            //invalid hop, decrement counter
            rip_table[i].ttl_flush--;
            if (rip_table[i].ttl_flush == 0){
                rip_table[i].valid = RIP_INVALID_ROW;
                xSemaphoreGive(rip_table[i].row_sem);
                continue;
            }
        }

        // //test to ensure routing works
        // if (rip_table[i].info.board_id == 25){
        //     rip_message[message_idx++] = 25;
        //     rip_message[message_idx++] = 10;
        // } else {
        rip_message[message_idx++] = rip_table[i].info.board_id;
        rip_message[message_idx++] = rip_table[i].info.hops;
        // }

        xSemaphoreGive(rip_table[i].row_sem);
    }

    esp_err_t res;
    if (broadcast){
        res = send(BROADCAST_ADDR, rip_message, message_idx, FrameType::RIP_TABLE_CONTROL, 0);
    } else {
        ESP_LOGI(DEBUG_LINK_TAG, "replying to discovery request to board %d", dest_id);
        res = send(dest_id, rip_message, message_idx, FrameType::RIP_TABLE_CONTROL, FLAG_DISCOVERY);
    }
    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to send rip frame on channel %d", 0);
    }

    return ESP_OK;
}

/**
 * @brief Determines which channel to route the frame to, depending on the dest (board) id
 * 
 * @param dest_id 
 * @param channel_to_send 
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::route_frame(uint8_t dest_id, uint8_t* channel_to_send){
    RIPRow* entry = nullptr;

    esp_err_t res;

    res = rip_find_entry(dest_id, &entry, false);
    if (entry == nullptr){
        return ESP_FAIL;
    }

    if (res != ESP_OK){
        return res;
    }

    *channel_to_send = entry->channel;

    return ESP_OK;
}

esp_err_t DataLinkManager::get_routing_table(RIPRow_public* table, size_t* table_size){
    if (table == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid table pointer");
        return ESP_FAIL;
    }

    if (table_size == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid table size pointer");
        return ESP_FAIL;
    }

    if (*table_size < RIP_MAX_ROUTES){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid table size (must be greater than %d)", RIP_MAX_ROUTES);
        return ESP_FAIL;
    }

    size_t curr_size = 0;

    for (size_t i = 0; i < RIP_MAX_ROUTES; i++){
        if (xSemaphoreTake(rip_table[i].row_sem, (TickType_t)RIP_MAX_SEM_WAIT) != pdTRUE){
            return ESP_FAIL;
        }
        if (rip_table[i].valid == RIP_VALID_ROW){
            table[i].info = rip_table[i].info;
            table[i].channel = rip_table[i].channel;
            curr_size++;
        } 
        xSemaphoreGive(rip_table[i].row_sem);
    }

    *table_size = curr_size;

    return ESP_OK;
}

/**
 * @brief Gets all of the routing tables of each board in the network and returns a routing matrix (entire topology of the network).
 * 
 * RIP table will have an entry that refers to its own board id (and will always have hop value of 0 and a channel value of `MAX_CHANNELS + 1`)
 * 
 * @warning not completely working (unable to get other board's table properly)
 * 
 * @param matrix 
 * @param matrix_size size in multiples of `sizeof(RIPRow_public)`
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::get_network_toplogy(RIPRow_public_matrix* matrix, size_t* matrix_size){
    if (matrix == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid matrix pointer");
        return ESP_FAIL;
    }

    if (matrix_size == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid matrix size pointer");
        return ESP_FAIL;
    }

    if (*matrix_size < RIP_MAX_ROUTES){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid matrix size (must be greater than %d)", RIP_MAX_ROUTES);
        return ESP_FAIL;
    }

    size_t curr_size = 0;
    matrix[0].board_id = this_board_id;
    if (matrix[0].table == nullptr || matrix[0].size < RIP_MAX_ROUTES){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid table size for index 0");
        return ESP_FAIL;
    }

    esp_err_t res;
    
    res = get_routing_table(matrix[0].table, &matrix[0].size);
    if (res != ESP_OK){
        return ESP_FAIL;
    }
    curr_size++;

    uint8_t message[RIP_DISCOVERY_MESSAGE_SIZE] = {0};
    for (size_t i = 1; i < matrix[0].size; i++){
        ESP_LOGI(DEBUG_LINK_TAG, "Sending discovery request for board %d", matrix[0].table[i].info.board_id);
        send(matrix[0].table[i].info.board_id, message, 1, FrameType::RIP_TABLE_CONTROL, FLAG_DISCOVERY); //send a discovery request to a board in this board's table index i
        uint8_t table_idx = 0;
        RIPRow_public temp;
        while (xQueueReceive(discovery_tables, &temp, (TickType_t)1000) == pdTRUE){ //the board should have responded with rows from its routing table to insert into the matrix
            ESP_LOGI(DEBUG_LINK_TAG, "putting discovery reply into matrix");
            matrix[i].table[table_idx].info.board_id = temp.info.board_id;
            matrix[i].table[table_idx].info.hops = temp.info.hops;
            matrix[i].table[table_idx++].channel = temp.channel;
        }
        matrix[i].size = table_idx;
        curr_size++;

        xQueueReset(discovery_tables); //reset the queue
    }

    *matrix_size = curr_size;

    return ESP_OK;
}

[[noreturn]] void DataLinkManager::rip_broadcast_timer_function(void* args){
    DataLinkManager* link_layer_obj = static_cast<DataLinkManager*>(args);
    if (link_layer_obj == nullptr || link_layer_obj->manual_broadcasts == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "RIP Broadacst task failed to start due to invalid pointer");
        vTaskDelete(nullptr);
    }

    ESP_LOGI(DEBUG_LINK_TAG, "Starting RIP broadcast task");

    esp_err_t res;
    while(true){
        bool dummy;
        xQueueReceive(link_layer_obj->manual_broadcasts, &dummy, pdMS_TO_TICKS(RIP_BROADCAST_INTERVAL)); //wait up to RIP_BROADCAST_INTERVAL ms
        ESP_LOGI(DEBUG_LINK_TAG, "Broadcasting table..."); //debug
        res = link_layer_obj->send_rip_frame(true, 0);
        if (res != ESP_OK){
            ESP_LOGE(DEBUG_LINK_TAG, "Failed to broadcast rip frame");
        }
    }
}

[[noreturn]] void DataLinkManager::rip_ttl_decrement_task(void* args){
    DataLinkManager* link_layer_obj = static_cast<DataLinkManager*>(args);
    if (link_layer_obj == nullptr || link_layer_obj->manual_broadcasts == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "RIP Broadacst task failed to start due to invalid pointer");
        vTaskDelete(nullptr);
    }
    ESP_LOGI(DEBUG_LINK_TAG, "Starting RIP ttl decrement task");
    bool broadcast = false;
    bool dummy = true;
    xQueueSend(link_layer_obj->manual_broadcasts, &dummy, 0);
    while(true){
        vTaskDelay(pdMS_TO_TICKS(RIP_MS_TO_SEC)); //run every second
        for (size_t i = 0; i < RIP_MAX_ROUTES; i++){
            // ESP_LOGI(DEBUG_LINK_TAG, "Decrementing ttl on entry %d", i);
            if (xSemaphoreTake(link_layer_obj->rip_table[i].row_sem, (TickType_t)RIP_MAX_SEM_WAIT) !=pdTRUE){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to get sem from entry %d", i);
                continue;
            }
            if (link_layer_obj->rip_table[i].valid == RIP_INVALID_ROW){
                xSemaphoreGive(link_layer_obj->rip_table[i].row_sem);
                continue;
            }
            if (link_layer_obj->rip_table[i].ttl != 0){
                // link_layer_obj->rip_table[i].valid = RIP_INVALID_ROW;
                // ESP_LOGI(DEBUG_LINK_TAG, "Entry %d now has ttl %d", i, link_layer_obj->rip_table[i].ttl);
            // } else {
                link_layer_obj->rip_table[i].ttl--;
                if (link_layer_obj->rip_table[i].ttl == 0){
                    link_layer_obj->rip_table[i].info.hops = RIP_MAX_HOPS + 1;
                    link_layer_obj->rip_table[i].ttl_flush = RIP_FLUSH_COUNT;
                    broadcast = true;
                }
            } 

            xSemaphoreGive(link_layer_obj->rip_table[i].row_sem);
        }

        if (broadcast && uxQueueMessagesWaiting(link_layer_obj->manual_broadcasts) == 0){
            broadcast = false;
            xQueueSend(link_layer_obj->manual_broadcasts, &dummy, 0);
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
    manual_broadcasts = xQueueCreate(2, sizeof(bool));

    ESP_LOGI(DEBUG_LINK_TAG, "Starting RIP Broadcast task");
    xTaskCreate(DataLinkManager::rip_broadcast_timer_function, "RIPBroadcast", 4096, static_cast<void*>(this), 5, NULL);
    ESP_LOGI(DEBUG_LINK_TAG, "Starting RIP TTL task");
    xTaskCreate(DataLinkManager::rip_ttl_decrement_task, "RIPTTL", 2048, static_cast<void*>(this), 5, NULL);
} 