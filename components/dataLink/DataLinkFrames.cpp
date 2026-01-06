#include "DataLinkManager.h"
#include "esp_log.h"

/**
 * @brief Creates a Control Frame from `FrameHeader`
 * 
 * @param header 
 * @return ControlFrame 
 */
ControlFrame make_control_frame_from_header(const FrameHeader& header) {
    ControlFrame frame{};
    frame.preamble = header.preamble;
    frame.sender_id = header.sender_id;
    frame.receiver_id = header.receiver_id;
    frame.seq_num = header.seq_num;
    frame.type_flag = header.type_flag;
    frame.data_len = header.data_len;
    frame.crc_16 = header.crc_16;
    return frame;
}

/**
 * @brief Creates a Generic Frame from `FrameHeader`
 * 
 * @param header 
 * @return GenericFrame 
 */
GenericFrame make_generic_frame_from_header(const FrameHeader& header) {
    GenericFrame frame{};
    frame.preamble = header.preamble;
    frame.sender_id = header.sender_id;
    frame.receiver_id = header.receiver_id;
    frame.seq_num = header.seq_num;
    frame.type_flag = header.type_flag;
    frame.total_frag = (header.frag_info >> 16) & 0xFFFF;
    frame.frag_num = (header.frag_info) & 0xFFFF;
    frame.data_len = header.data_len;
    frame.crc_16 = header.crc_16;
    return frame;
}

/**
 * @brief Store a fragment that has been received 
 * 
 * @param fragment 
 * @param channel
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::store_fragment(GenericFrame* fragment, uint8_t channel){
    if (fragment == nullptr){
        return ESP_ERR_INVALID_ARG;
    }

    if (fragment->data_len == 0){
        return ESP_ERR_INVALID_ARG;
    }

    if (fragment->receiver_id != this_board_id){
        return ESP_ERR_INVALID_ARG;
    }

    // ESP_LOGI(DEBUG_LINK_TAG, "got frame %d, fragment %d of %d", fragment->seq_num, fragment->frag_num, fragment->total_frag);

    if (rx_fragment_mutex[channel] == NULL){
        return ESP_FAIL;
    }
    
    if (xSemaphoreTake(rx_fragment_mutex[channel], pdMS_TO_TICKS(ASYNC_QUEUE_WAIT_TICKS)) != pdTRUE){
        return ESP_ERR_TIMEOUT;
    }

    if (fragment_map[channel][fragment->receiver_id].find(fragment->seq_num) == fragment_map[channel][fragment->receiver_id].end()){
        FragmentMetadata& metadata = fragment_map[channel][fragment->receiver_id][fragment->seq_num];
        metadata.num_fragments_rx = 0;

        metadata.fragments.reserve(fragment->total_frag);
        for (uint16_t i = 0; i < fragment->total_frag; i++){
            metadata.fragments.push_back(GenericFrame{});
        }
    }

    FragmentMetadata& metadata = fragment_map[channel][fragment->receiver_id][fragment->seq_num];
    if ((fragment->frag_num-1) > metadata.fragments.size()){
        xSemaphoreGive(rx_fragment_mutex[channel]);
        return ESP_ERR_INVALID_STATE;
    }

    if (metadata.fragments[fragment->frag_num-1].data_len == 0){
        metadata.fragments[fragment->frag_num-1] = *fragment;
        metadata.num_fragments_rx++;
        // ESP_LOGI(DEBUG_LINK_TAG, "store frame %d fragment %d success; got %d out of %d", fragment->seq_num, fragment->frag_num, metadata.num_fragments_rx, metadata.fragments.size());
    }

    uint16_t last_consec_rx_frag = 0;
    if (static_cast<FrameType>(GET_TYPE(fragment->type_flag)) != FrameType::MISC_UDP_GENERIC_TYPE){
        for (; last_consec_rx_frag < fragment->total_frag; last_consec_rx_frag++){
            if (metadata.fragments[last_consec_rx_frag].data_len == 0){
                //found missing fragment
                break;
            }
        }
    }

    size_t metadata_fragment_size = metadata.fragments.size();
    xSemaphoreGive(rx_fragment_mutex[channel]);

    if (static_cast<FrameType>(GET_TYPE(fragment->type_flag)) != FrameType::MISC_UDP_GENERIC_TYPE){
        SendAckMetaData data = {
            .data = {GENERIC_FRAG_ACK_PREAMBLE, static_cast<uint8_t>((last_consec_rx_frag & 0xFF00) >> 8), static_cast<uint8_t>(last_consec_rx_frag & 0xFF), 
            static_cast<uint8_t>((fragment->total_frag & 0xFF00) >> 8), static_cast<uint8_t>(fragment->total_frag & 0xFF), 
            static_cast<uint8_t>((fragment->seq_num & 0xFF00) >> 8), static_cast<uint8_t>(fragment->seq_num & 0xFF)},
            .sender_id = fragment->sender_id,
        };
        if (xSemaphoreTake(send_ack_queue_mutex[channel], pdMS_TO_TICKS(SEND_ACK_MUTEX_WAIT)) != pdTRUE){
            return ESP_FAIL;
        }

        send_ack_queue[channel].push(data);
        xSemaphoreGive(send_ack_queue_mutex[channel]);
    }

    if (metadata.num_fragments_rx == metadata_fragment_size){
        //all fragments received
        return complete_fragment(fragment->receiver_id, fragment->seq_num, channel);
    }

    return ESP_OK;
}

/**
 * @brief Removes the corresponding entry from `fragment_map` and pushes the data onto `async_receive_queue`
 * 
 * @param board_id
 * @param sequence_num
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::complete_fragment(uint16_t board_id, uint16_t sequence_num, uint8_t channel){
    Rx_Metadata rx;
    
    if (xSemaphoreTake(rx_fragment_mutex[channel], pdMS_TO_TICKS(ASYNC_QUEUE_WAIT_TICKS)) != pdTRUE){
        return ESP_ERR_TIMEOUT;
    }

    if (fragment_map[channel][board_id].find(sequence_num) == fragment_map[channel][board_id].end()){
        return ESP_ERR_NOT_FOUND;
    }

    FragmentMetadata& metadata = fragment_map[channel][board_id][sequence_num];
    if (metadata.num_fragments_rx != metadata.fragments.size()){
        return ESP_ERR_INVALID_STATE;
    }
    uint16_t total_data_len = metadata.num_fragments_rx*MAX_FRAME_SIZE; //max data size with n fragments
    xSemaphoreGive(rx_fragment_mutex[channel]);
    
    uint8_t* combined_data = (uint8_t*)pvPortMalloc(total_data_len);
    rx.data_len = total_data_len;
    if (combined_data == nullptr){
        return ESP_ERR_NO_MEM;
    }

    if (rx_fragment_mutex[channel] == NULL){
        return ESP_FAIL;
    }

    // ESP_LOGI(DEBUG_LINK_TAG, "completing %d fragments for frame %d", metadata.num_fragments_rx, sequence_num);

    if (xSemaphoreTake(rx_fragment_mutex[channel], pdMS_TO_TICKS(ASYNC_QUEUE_WAIT_TICKS)) != pdTRUE){
        return ESP_ERR_TIMEOUT;
    }
    
    if (fragment_map[channel][board_id].find(sequence_num) == fragment_map[channel][board_id].end()){
        xSemaphoreGive(rx_fragment_mutex[channel]);
        return ESP_ERR_NOT_FOUND;
    }

    rx.data = combined_data;
    uint16_t prev_index = 0;
    for (size_t i = 0; i < metadata.num_fragments_rx; i++){
        memcpy(&combined_data[prev_index], metadata.fragments[i].data, metadata.fragments[i].data_len);
        prev_index += metadata.fragments[i].data_len;
    }

    xSemaphoreGive(rx_fragment_mutex[channel]);

    rx.data_len = prev_index;

    if (async_rx_queue_mutex[channel] == nullptr){
        return ESP_FAIL;
    }

    GenericFrame frame = metadata.fragments[0];

    rx.header = {
        .preamble = START_OF_FRAME,
        .sender_id = frame.sender_id,
        .receiver_id = frame.receiver_id,
        .seq_num = frame.seq_num,
        .type_flag = frame.type_flag,
        .frag_info = (uint32_t)((frame.total_frag << 16) | frame.frag_num),
        .data_len = prev_index,
        .crc_16 = 0
    };

    // ESP_LOGI(DEBUG_LINK_TAG, "pushing frame %d onto async rx queue", sequence_num);

    if (xSemaphoreTake(async_rx_queue_mutex[channel], pdMS_TO_TICKS(ASYNC_QUEUE_WAIT_TICKS)) != pdTRUE){
        vPortFree(combined_data);
        return ESP_ERR_TIMEOUT;
    }

    async_receive_queue[channel].push(rx);

    xSemaphoreGive(async_rx_queue_mutex[channel]);

    fragment_map[channel][board_id].erase(sequence_num);
    
    if (fragment_map[channel][board_id].empty()) {
        fragment_map[channel].erase(board_id);
    }

    // ESP_LOGI(DEBUG_LINK_TAG, "frame %d pushed success", sequence_num);

    return ESP_OK;
}

/**
 * @brief Sends an ACK
 * 
 * @param sender_id This is the board id that is receiving the ACK (the original sender board id)
 * @param data
 * @param data_len
 * @return esp_err_t 
 * 
 * @note This may be moved to a private function - Unsure if users should be able to manually send ACKs
 */
esp_err_t DataLinkManager::send_ack(uint8_t sender_id, uint8_t* data, uint16_t data_len){
    return send(sender_id, data, data_len, FrameType::ACK_TYPE, 0x0);
}

/**
 * @brief Checks the channel receive queue for any received frames. If there is, return the first frame's data size
 * 
 * @param frame_size Size of the data
 * @param header Header information of the combined generic frames
 * 
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::async_receive_info(uint16_t* frame_size, FrameHeader* header, uint8_t channel){
    if (frame_size == nullptr || header == nullptr){
        return ESP_ERR_INVALID_ARG;
    }

    Rx_Metadata top;
    if (xSemaphoreTake(async_rx_queue_mutex[channel], pdMS_TO_TICKS(ASYNC_QUEUE_WAIT_TICKS)) != pdTRUE){
        return ESP_ERR_TIMEOUT;
    }
    if (async_receive_queue[channel].size() == 0){
        xSemaphoreGive(async_rx_queue_mutex[channel]);

        *frame_size = 0;

        return ESP_OK;
    }
    top = async_receive_queue[channel].front();

    xSemaphoreGive(async_rx_queue_mutex[channel]);

    *frame_size = top.data_len;

    *header = top.header;

    return ESP_OK;
}

/**
 * @brief Get the first frame's data
 * 
 * @param data Char array of the actual combined data
 * @param data_len Combined data length
 * @param header Header information of returning frame
 * 
 */
esp_err_t DataLinkManager::async_receive(uint8_t* data, uint16_t data_len, FrameHeader* header, uint8_t channel){
    if (data == nullptr || header == nullptr){
        return ESP_ERR_INVALID_ARG;
    }

    if (data_len == 0){
        return ESP_ERR_INVALID_ARG;
    }

    Rx_Metadata top;
    if (xSemaphoreTake(async_rx_queue_mutex[channel], pdMS_TO_TICKS(ASYNC_QUEUE_WAIT_TICKS)) != pdTRUE){
        return ESP_ERR_TIMEOUT;
    }

    if (async_receive_queue[channel].size() == 0){
        xSemaphoreGive(async_rx_queue_mutex[channel]);
        return ESP_ERR_NOT_FOUND;
    }

    top = async_receive_queue[channel].front();
    async_receive_queue[channel].pop();

    xSemaphoreGive(async_rx_queue_mutex[channel]);
    
    if (data_len < top.data_len){
        vPortFree(top.data);
        return ESP_ERR_INVALID_ARG;
    }

    *header = top.header;

    memcpy(data, top.data, top.data_len);
    vPortFree(top.data);

    // ESP_LOGI(DEBUG_LINK_TAG, "pushed frame %d onto async queue", header->seq_num);

    return ESP_OK;
}

esp_err_t DataLinkManager::receive_rmt(uint8_t channel){
    uint16_t data_len = MAX_FRAME_SIZE; //max possible data len
    uint8_t data[data_len];
    memset(data, 0, data_len);

    size_t recv_len = 0;

    esp_err_t res = phys_comms->receive(data, data_len, &recv_len, channel);

    if (res != ESP_OK){
        // ESP_LOGE(DEBUG_LINK_TAG, "RMT Failed to receive - recieve_rmt");
        return ESP_ERR_TIMEOUT;
    }
    
    if (recv_len > MAX_FRAME_SIZE){
        ESP_LOGE(DEBUG_LINK_TAG, "Received frame is too large to be control or generic");
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (recv_len < CONTROL_FRAME_OVERHEAD) {
        //Frame is too small
        return ESP_ERR_INVALID_RESPONSE;
    }

    uint8_t message[MAX_FRAME_SIZE];
    memset(message, 0, sizeof(message));
    size_t message_size = 0;
    FrameHeader header;

    res = get_data_from_frame(data, recv_len, message, &message_size, &header);
    if (res != ESP_OK){
        // print_buffer_binary(message, message_size);
        return res;
    }

    // print_buffer_binary(message, message_size);

    //push control frame onto async_receive_queue
    if (static_cast<FrameType>(GET_TYPE(header.type_flag)) == FrameType::ACK_TYPE){
        if (message_size != GENERIC_FRAG_ACK_DATA_SIZE || message_size == 0){
            return ESP_OK;
        }
        
        if (message[0] != GENERIC_FRAG_ACK_PREAMBLE){
            return ESP_OK;
        }

        FrameAckRecord record = {
            .last_ack = static_cast<uint16_t>((message[1] << 8) | (message[2])),
            .total_frags = static_cast<uint16_t>((message[3] << 8) | (message[4])),
            .seq_num = static_cast<uint16_t>((message[5] << 8) | (message[6]))
        };
        
        res = inc_head_sliding_window(channel, header.sender_id, record.seq_num, &record);

        // if (res == ESP_OK){
        //     ESP_LOGI(DEBUG_LINK_TAG, "Got ACK for seq number %d from board %d! Highest Conseq ACK: 0x%X%X Total Frag: 0x%X%X", record.seq_num, header.sender_id, message[1], message[2], message[3], message[4]);
        // } else {
        //     ESP_LOGI(DEBUG_LINK_TAG, "Got ACK for seq number %d from board %d but got a lower conseq ack 0x%x%X Total Frag: 0x%X%X", record.seq_num, header.sender_id, message[1], message[2], message[3], message[4]);
        // }
        
        return ESP_OK;
    }

    if (!IS_CONTROL_FRAME(header.type_flag)){
        //Handle generic frame fragment
        GenericFrame frame = make_generic_frame_from_header(header);
        if (message_size > MAX_FRAME_SIZE){
            return ESP_FAIL;
        }

        memcpy(frame.data, message, message_size);
        esp_err_t res = store_fragment(&frame, channel);
        return res;
    }

    //control frame handling: - TODO: clean up :)
    memcpy(data, message, message_size);
    // ESP_LOGI(DEBUG_LINK_TAG, "Received frame of type 0x%X destined for board %d", GET_TYPE(header.type_flag), header.receiver_id);
    
    //check for a rip frame
    if (static_cast<FrameType>(GET_TYPE(header.type_flag)) == FrameType::RIP_TABLE_CONTROL){
        ESP_LOGI(DEBUG_LINK_TAG, "Got a RIP frame");

        for (size_t i = 0; i < message_size-1; i+=2){
            uint8_t board_id = message[i];
            uint8_t hops = message[i+1];
            // ESP_LOGI(DEBUG_LINK_TAG, "Received: board_id %d and number of hops %d on channel %d", board_id, hops, channel);
            
            RIPRow* entry = nullptr;
            
            res = rip_find_entry(board_id, &entry, true);
            if (res != ESP_OK){
                return ESP_FAIL;
            }
            
            if (entry == nullptr){
                printf("rip pointer\n");
                return ESP_FAIL; //no room for more entries in the table
            }
            
            if (entry->valid == RIP_NEW_ROW){
                //adding a new entry
                rip_add_entry(board_id, hops + 1, channel, &entry);
            } else {
                //updating an entry
                rip_update_entry(hops + 1, channel, &entry);
            }
            
            if (GET_FLAG(header.type_flag) == FLAG_DISCOVERY){
                //discovery -> send routing table
                // ESP_LOGI(DEBUG_LINK_TAG, "got discovery reply");
                RIPRow_public row_queue = {
                    .info = entry->info,
                    .channel = entry->channel
                };
                
                xQueueSendToBack(discovery_tables, &row_queue, (TickType_t)10);
            }
            
        }
        if (message_size == RIP_DISCOVERY_MESSAGE_SIZE){
            res = send_rip_frame(false, header.sender_id);
            if (res != ESP_OK){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to send back rip table to board %d", header.sender_id);
                return res;
            }
        }

        return res;
    }

    uint16_t seq_num = 0;

    res = get_sequence_num(header.receiver_id, &seq_num);

    if (res != ESP_OK){
        return res;
    }

    //got frame but not destined for this board
    if (header.receiver_id != this_board_id && header.receiver_id != BROADCAST_ADDR && header.seq_num > seq_num){
        // ESP_LOGI(DEBUG_LINK_TAG, "Sending message to board %d with message %s", header.receiver_id, message);
        res = send(header.receiver_id, message, message_size, FrameType::MISC_CONTROL_TYPE, 0);
        return res;
    }

    uint8_t* metadata_message = (uint8_t*)pvPortMalloc(message_size);
    if (metadata_message == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to malloc for receive");
        return ESP_ERR_NO_MEM;
    }
    memcpy(metadata_message, message, message_size);

    Rx_Metadata metadata = {
        .data = metadata_message,
        .data_len = (uint16_t)message_size,
        .header = header
    };

    if (xSemaphoreTake(async_rx_queue_mutex[channel], pdMS_TO_TICKS(ASYNC_QUEUE_WAIT_TICKS)) == pdTRUE){
        async_receive_queue[channel].push(metadata);
        xSemaphoreGive(async_rx_queue_mutex[channel]);
    } else {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

[[noreturn]] void DataLinkManager::receive_thread_main(void* args){
    DataLinkManager* link_layer_obj = static_cast<DataLinkManager*>(args);
    if (link_layer_obj == nullptr || link_layer_obj->manual_broadcasts == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Receive thread failed to start due to invalid pointer");
        vTaskDelete(nullptr);
    }

    ESP_LOGI(DEBUG_LINK_TAG, "Starting Receive thread task");

    esp_err_t res;

    for (uint8_t i = 0; i < link_layer_obj->num_channels; i++){
        res = link_layer_obj->start_receive_frames_rmt(i);
    }
    while(!link_layer_obj->stop_tasks){
        for (uint8_t i = 0; i < link_layer_obj->num_channels; i++){
            res = link_layer_obj->receive_rmt(i);
            res = link_layer_obj->start_receive_frames_rmt(i);
        }
        
        vTaskDelay(pdMS_TO_TICKS(RECEIVE_TASK_PERIOD_MS));
    }

    vTaskDelete(nullptr);
}

[[noreturn]] void DataLinkManager::send_ack_thread_main(void* args){
    DataLinkManager* link_layer_obj = static_cast<DataLinkManager*>(args);
    if (link_layer_obj == nullptr || link_layer_obj->manual_broadcasts == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Send Ack thread failed to start due to invalid pointer");
        vTaskDelete(nullptr);
    }

    for (uint8_t i = 0; i < link_layer_obj->num_channels; i++){
        if (link_layer_obj->send_ack_queue_mutex[i] == NULL){
            ESP_LOGE(DEBUG_LINK_TAG, "%d send ack queue mutex is null!", i);
            vTaskDelete(nullptr);
        }
    }

    ESP_LOGI(DEBUG_LINK_TAG, "Starting Send ACK task");

    esp_err_t res;

    while(!link_layer_obj->stop_tasks){
        for (uint8_t channel = 0; channel < link_layer_obj->num_channels; channel++){
            vTaskDelay(pdMS_TO_TICKS(SEND_ACK_PERIOD_MS));

            if (xSemaphoreTake(link_layer_obj->send_ack_queue_mutex[channel], pdMS_TO_TICKS(SEND_ACK_MUTEX_WAIT)) != pdTRUE){
                continue;
            }
    
            if (link_layer_obj->send_ack_queue[channel].empty()){
                xSemaphoreGive(link_layer_obj->send_ack_queue_mutex[channel]);
                continue;
            }
    
            SendAckMetaData data = link_layer_obj->send_ack_queue[channel].front();
            link_layer_obj->send_ack_queue[channel].pop();
            link_layer_obj->send_ack(data.sender_id, data.data, GENERIC_FRAG_ACK_DATA_SIZE);
            xSemaphoreGive(link_layer_obj->send_ack_queue_mutex[channel]);
        }
    }

    vTaskDelete(nullptr);
}