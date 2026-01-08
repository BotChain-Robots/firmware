#include "DataLinkManager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "esp_random.h"

void DataLinkManager::init_scheduler(){
    for (int i = 0; i < num_channels; i++){
        sq_handle[i] = xSemaphoreCreateMutex();
        async_rx_queue_mutex[i] = xSemaphoreCreateMutex();
        rx_fragment_mutex[i] = xSemaphoreCreateMutex();
        sliding_window_mutex[i] = xSemaphoreCreateMutex();
        send_ack_queue_mutex[i] = xSemaphoreCreateMutex();
    }

    ESP_LOGI(DEBUG_LINK_TAG, "Starting Frame Scheduler task");
    xTaskCreate(DataLinkManager::frame_scheduler, "Scheduler", 4096, static_cast<void*>(this), 4, &scheduler_task);
    xTaskCreate(DataLinkManager::receive_thread_main, "Receiver", 8192, static_cast<void*>(this), 5, &receive_task);
    xTaskCreate(DataLinkManager::send_ack_thread_main, "Send ACKs", 8192, static_cast<void*>(this), 5, &send_ack_task);
}

/**
 * @brief Schedules which frame to send
 *
 * Scheduler:
 * - All frames will be pushed to the back onto a queue
 * - When a generic frame sends a chunk, it will be pushed back to the queue for the next chunk to be sent
 *
 * Scheduling may change (above scheduler will lead to starvation of control frames depending on the number of generic frames/fragments to send)
 */
[[noreturn]] void DataLinkManager::frame_scheduler(void* args){
    DataLinkManager* link_layer_obj = static_cast<DataLinkManager*>(args);
    if (link_layer_obj == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Frame Scheduler failed to start due to invalid pointer");
        vTaskDelete(nullptr);
    }

    ESP_LOGI(DEBUG_LINK_TAG, "Starting Frame Scheduler task");
    while(!link_layer_obj->stop_tasks){
        for (uint8_t i = 0; i < link_layer_obj->num_channels; i++){
            link_layer_obj->scheduler_send(i);
        }
        vTaskDelay(pdMS_TO_TICKS(SCHEDULER_PERIOD_MS));

    }
    vTaskDelete(nullptr);
}

/**
 * @brief Pushes a frame to the scheduler
 *
 * @param frame
 * @param channel
 * @return esp_err_t
 */
esp_err_t DataLinkManager::push_frame_to_scheduler(SchedulerMetadata frame, uint8_t channel){
    if (frame.data == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Frame data is null");
        return ESP_ERR_INVALID_ARG;
    }

    if (channel >= num_channels){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid channels");
        return ESP_ERR_INVALID_ARG;
    }

    if (frame.len == 0){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid Frame Length");

        return ESP_ERR_INVALID_ARG;
    }

    int64_t now = esp_timer_get_time();
    frame.enqueue_time_ns = now;

    if (sq_handle[channel] == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid scheduler queue handle");
        return ESP_FAIL;
    }

    if (xSemaphoreTake(sq_handle[channel], pdMS_TO_TICKS(SCHEDULER_MUTEX_WAIT)) == pdTRUE){
        frame_queue[channel].push(frame);
        xSemaphoreGive(sq_handle[channel]);
    } else {
        //Failed to obtain mutex
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to get mutex");
        return ESP_ERR_TIMEOUT;
    }

    // ESP_LOGI(DEBUG_LINK_TAG, "Pushed frame to queue on channel %d", channel);

    return ESP_OK;
}

/**
 * @brief Scheduler sending the actual frame at the top of the heap on a channel
 *
 * @return esp_err_t
 */
esp_err_t DataLinkManager::scheduler_send(uint8_t channel){
    if (phys_comms == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to send frame due to no RMT object");
        return ESP_ERR_INVALID_ARG;
    }

    if (sq_handle[channel] == nullptr){
        return ESP_FAIL;
    }

    SchedulerMetadata frame;

    if (xSemaphoreTake(sq_handle[channel], pdMS_TO_TICKS(SCHEDULER_MUTEX_WAIT)) == pdTRUE){
        if (frame_queue[channel].empty()){
            xSemaphoreGive(sq_handle[channel]);
            // ESP_LOGI(DEBUG_LINK_TAG, "Scheduler queue for channel %d is empty", channel);
            return ESP_OK;
        }
        frame = frame_queue[channel].top();
        frame_queue[channel].pop();
        xSemaphoreGive(sq_handle[channel]);
    } else {
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to get mutex when trying to send");
        //Failed to obtain mutex
        return ESP_ERR_TIMEOUT;
    }

    if (frame.data == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Data array does not exist");
        return ESP_ERR_INVALID_ARG;
    }

    if (this_board_id == PC_ADDR){
        ESP_LOGE(DEBUG_LINK_TAG, "This board is not assigned a board id");
        vPortFree(frame.data);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t res;
    bool isControlFrame = IS_CONTROL_FRAME(static_cast<uint8_t>(frame.header.type_flag));

    size_t frame_size = isControlFrame ? sizeof(ControlFrame) : sizeof(GenericFrame);
    uint8_t send_data[frame_size];

    if (isControlFrame){
        //control frame

        res = create_control_frame(frame.data, frame.len,
            make_control_frame_from_header(frame.header), send_data, &frame_size);

        vPortFree(frame.data);

        if (res != ESP_OK){
            ESP_LOGE(DEBUG_LINK_TAG, "Failed to create control frame");
            return res;
        }

        // ESP_LOGI(DEBUG_LINK_TAG, "Sending control frame with %d bytes", frame_size);
        return scheduler_send_rmt(channel, frame, send_data, frame_size, false);
    } else {
        //generic frame
        if (frame.len > (MAX_GENERIC_DATA_LEN)){
            //fragment here

            if (frame.timeout == 0){
                frame.timeout = GENERIC_FRAME_MIN_TIMEOUT;
            } else {
                frame.timeout--;
                res = push_frame_to_scheduler(frame, channel);
                if (res != ESP_OK){
                    ESP_LOGE(DEBUG_LINK_TAG, "Failed to schedule next generic frame fragment");
                    vPortFree(frame.data);
                    return res;
                }
                return ESP_OK;
            }

            if (static_cast<FrameType>(GET_TYPE(frame.header.type_flag)) != FrameType::MISC_UDP_GENERIC_TYPE){
                FrameAckRecord record = {
                    .last_ack = 0,
                    .total_frags = 0
                };

                res = get_record_sliding_window(channel, frame.header.receiver_id, frame.header.seq_num, &record);

                if (res != ESP_OK){
                    ESP_LOGE(DEBUG_LINK_TAG, "Failed to get sliding window ack record for board id %d seq num %d", frame.header.receiver_id, frame.header.seq_num);
                    vPortFree(frame.data);
                    return res;
                }

                if (record.last_ack == 0 && record.total_frags == 0){
                    //no acks has arrived yet - can only send fragment 1 ... fragment GENERIC_FRAME_SLIDING_WINDOW_SIZE (inclusive)
                    // ESP_LOGI(DEBUG_LINK_TAG, "no ack received yet for board id %d seq num %d", frame.header.receiver_id, frame.header.seq_num);
                    if (frame.curr_fragment < GENERIC_FRAME_SLIDING_WINDOW_SIZE){
                        frame.curr_fragment++;
                    } else {
                        frame.curr_fragment = 1;
                    }
                } else {
                    //some acks has arrived
                    // ESP_LOGI(DEBUG_LINK_TAG, "some ack received for board id %d seq num %d last ack %d total frags %d", frame.header.receiver_id, frame.header.seq_num, record.last_ack, record.total_frags);

                    //check if all acks are received
                    frame.last_ack = record.last_ack;
                    if (record.last_ack == record.total_frags){
                        //all acks received, can simply exit
                        // ESP_LOGI(DEBUG_LINK_TAG, "All acks recevied for board id %d seq num %d", frame.header.receiver_id, frame.header.seq_num);
                        complete_record_sliding_window(channel, frame.header.receiver_id, frame.header.seq_num);
                        vPortFree(frame.data);
                        return ESP_OK;
                    }

                    if (frame.curr_fragment - frame.last_ack < GENERIC_FRAME_SLIDING_WINDOW_SIZE && frame.curr_fragment - frame.last_ack < record.total_frags){
                        frame.curr_fragment++;
                    } else {
                        frame.curr_fragment = frame.last_ack;
                    }
                }
            } else {
                frame.curr_fragment++;
            }

            // ESP_LOGI(DEBUG_LINK_TAG, "current fragment to be sent for seq num %d is %d", frame.header.seq_num, frame.curr_fragment);

            //calculate data offset from curr_fragment
            uint16_t fragment_size = 0;

            if (frame.curr_fragment != (frame.header.frag_info >> 16)) {
                fragment_size = MAX_GENERIC_DATA_LEN;
            } else {
                fragment_size = frame.len - (MAX_GENERIC_DATA_LEN * (frame.curr_fragment-1));
            }

            uint16_t curr_offset = MAX_GENERIC_DATA_LEN * (frame.curr_fragment - 1);

            // ESP_LOGI(DEBUG_LINK_TAG, "frame %d curr offset %d\n", frame.header.seq_num, curr_offset);
            // ESP_LOGI(DEBUG_LINK_TAG, "frame %d fragment size %d\n", frame.header.seq_num, fragment_size);

            frame.header.frag_info = (frame.header.frag_info & 0xFFFF0000) | frame.curr_fragment; //increment frag_num
            //create fragment
            res = create_generic_frame(frame.data, fragment_size,
                make_generic_frame_from_header(frame.header), curr_offset, send_data, &frame_size);

            if (res != ESP_OK){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to create generic frame fragment");
                vPortFree(frame.data);
                return res;
            }

            res = scheduler_send_rmt(channel, frame, send_data, frame_size, true);

            if (res != ESP_OK){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to send generic frame fragment");
                res = push_frame_to_scheduler(frame, channel);
                if (res != ESP_OK){
                    ESP_LOGE(DEBUG_LINK_TAG, "Failed to schedule next generic frame fragment");
                    vPortFree(frame.data);
                }
                return res;
            }

            //need to schedule the next fragment (if total_frags != frag_num)
            if ((frame.header.frag_info >> 16) > (frame.header.frag_info & 0xFF) || (frame.last_ack != (frame.header.frag_info >> 16) &&
            static_cast<FrameType>(GET_TYPE(frame.header.type_flag)) != FrameType::MISC_UDP_GENERIC_TYPE)){
                // frame.generic_frame_data_offset += fragment_size;
                // ESP_LOGI(DEBUG_LINK_TAG, "scheduling frame %d with frag_info 0x%X", frame.header.seq_num, frame.header.frag_info);
                res = push_frame_to_scheduler(frame, channel);
                if (res != ESP_OK){
                    ESP_LOGE(DEBUG_LINK_TAG, "Failed to schedule next generic frame fragment");
                    vPortFree(frame.data);
                    return res;
                }
            } else {
                //Done fragmenting, can free data array
                // ESP_LOGI(DEBUG_LINK_TAG, "finished fragmenting seq num %d frag_info 0x%X", frame.header.seq_num, frame.header.frag_info);
                vPortFree(frame.data);
            }

        } else {
            //no fragmenting
            res = create_generic_frame(frame.data, frame.len,
                make_generic_frame_from_header(frame.header), 0, send_data, &frame_size);
            vPortFree(frame.data);

            if (res != ESP_OK){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to create generic frame");
                return res;
            }
            return scheduler_send_rmt(channel, frame, send_data, frame_size, false);
        }
    }

    return ESP_OK;
}

esp_err_t DataLinkManager::scheduler_send_rmt(uint8_t channel, SchedulerMetadata frame, uint8_t* send_data, size_t frame_size, bool wait_for_tx_done){
    esp_err_t res;
    uint8_t channel_to_route = MAX_CHANNELS;
    rmt_transmit_config_t config = {
        .loop_count = 0,
        .flags = {
            .eot_level = 0,   // typically 0 or 1, depending on your output idle level
        }
    };
    if (frame.header.receiver_id == BROADCAST_ADDR){
        // printf("Sending on channel %d\n", i);
        res = phys_comms->send(send_data, frame_size, &config, channel);
    } else {
        res = route_frame(frame.header.receiver_id, &channel_to_route);

        if (res != ESP_OK){
            ESP_LOGE(DEBUG_LINK_TAG, "Failed to find entry for %d", frame.header.receiver_id);
            return ESP_FAIL;
        }
        // ESP_LOGI(DEBUG_LINK_TAG, "Sending frame %d frag_info 0x%X", frame.header.seq_num, frame.header.frag_info);
        res = phys_comms->send(send_data, frame_size, &config, channel_to_route);
        // if (wait_for_tx_done){
        //     phys_comms->wait_until_send_complete(channel_to_route);
        // }
    }

    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to send message");
        return ESP_FAIL;
    } else{
        // ESP_LOGI(DEBUG_LINK_TAG, "Sent frame %d frag_info 0x%X", frame.header.seq_num, frame.header.frag_info);
    }

    return ESP_OK;
}

/**
 * @brief Increases the head of the sliding window associated with the board id and sequence number
 *
 * @param channel
 * @param board_id Receiving Board ID (the board who ACK'd)
 * @param seq_num
 * @param ack_record
 * @return esp_err_t
 */
esp_err_t DataLinkManager::inc_head_sliding_window(uint8_t channel, uint8_t board_id, uint16_t seq_num, FrameAckRecord* ack_record){
    if (ack_record == NULL){
        return ESP_ERR_INVALID_ARG;
    }

    if (ack_record->total_frags == 0 || ack_record->total_frags > MAX_GENERIC_NUM_FRAG
        || ack_record->last_ack == 0 || ack_record->total_frags < ack_record->last_ack){
        return ESP_ERR_INVALID_ARG;
    }

    if (sliding_window_mutex[channel] == NULL){
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(sliding_window_mutex[channel], pdMS_TO_TICKS(SLIDING_WINDOW_MUTEX_TIMEOUT_MS)) != pdTRUE){
        return ESP_ERR_TIMEOUT;
    }

    FrameAckRecord& record = sliding_window[channel][board_id][seq_num];

    if (record.last_ack > ack_record->last_ack){
        xSemaphoreGive(sliding_window_mutex[channel]);
        return ESP_ERR_INVALID_ARG;
    }

    record.last_ack = ack_record->last_ack;
    if (record.total_frags == 0){
        record.total_frags = ack_record->total_frags;
    }

    xSemaphoreGive(sliding_window_mutex[channel]);

    return ESP_OK;
}

/**
 * @brief Gets the current record associated with the board id and sequence number from the sliding window
 *
 * @param channel
 * @param board_id Receiving Board ID (the board who ACK'd)
 * @param seq_num
 * @param ack_record
 * @return esp_err_t
 */
esp_err_t DataLinkManager::get_record_sliding_window(uint8_t channel, uint8_t board_id, uint16_t seq_num, FrameAckRecord* ack_record){
    if (ack_record == NULL){
        return ESP_ERR_INVALID_ARG;
    }

    if (sliding_window_mutex[channel] == NULL){
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(sliding_window_mutex[channel], pdMS_TO_TICKS(SLIDING_WINDOW_MUTEX_TIMEOUT_MS)) != pdTRUE){
        return ESP_ERR_TIMEOUT;
    }

    if (sliding_window[channel][board_id].find(seq_num) == sliding_window[channel][board_id].end()){
        //record for this board id + seq number doesn't exist -- we don't want to create one
        xSemaphoreGive(sliding_window_mutex[channel]);
        ack_record->last_ack = 0;
        ack_record->total_frags = 0;
        return ESP_OK;
    }

    FrameAckRecord& record = sliding_window[channel][board_id][seq_num];

    ack_record->last_ack = record.last_ack;
    ack_record->total_frags = record.total_frags;

    xSemaphoreGive(sliding_window_mutex[channel]);

    return ESP_OK;
}

/**
 * @brief Removes the board id + sequence number record fromt the sliding window (map)
 *
 * @param channel
 * @param board_id Receiving Board ID (the board who ACK'd)
 * @param seq_num
 * @return esp_err_t
 */
esp_err_t DataLinkManager::complete_record_sliding_window(uint8_t channel, uint8_t board_id, uint16_t seq_num){
    if (sliding_window_mutex[channel] == NULL){
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(sliding_window_mutex[channel], pdMS_TO_TICKS(SLIDING_WINDOW_MUTEX_TIMEOUT_MS)) != pdTRUE){
        return ESP_ERR_TIMEOUT;
    }

    if (sliding_window[channel][board_id].find(seq_num) == sliding_window[channel][board_id].end()){
        //record for this board id + seq number doesn't exist -- we don't want to create one
        xSemaphoreGive(sliding_window_mutex[channel]);
        return ESP_ERR_INVALID_STATE;
    }

    sliding_window[channel][board_id].erase(seq_num);

    xSemaphoreGive(sliding_window_mutex[channel]);

    return ESP_OK;
}
