#include "DataLinkManager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/semphr.h"

void DataLinkManager::init_scheduler(){
    for (int i = 0; i < num_channels; i++){
        sq_handle[i] = xSemaphoreCreateMutex();
        async_rx_queue_mutex[i] = xSemaphoreCreateMutex();
    }

    ESP_LOGI(DEBUG_LINK_TAG, "Starting Frame Scheduler task");
    xTaskCreate(DataLinkManager::frame_scheduler, "Scheduler", 4096, static_cast<void*>(this), 5, &scheduler_task);
    xTaskCreate(DataLinkManager::receive_thread_main, "Scheduler", 8192, static_cast<void*>(this), 5, &receive_task);
}

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

        // ESP_LOGI(DEBUG_LINK_TAG, "Sending %d bytes", frame_size);
    } else {
        //generic frame
        
        if (frame.len > MAX_CONTROL_DATA_LEN){
            //fragment here
            uint16_t curr_offset = frame.generic_frame_data_offset;
            uint16_t fragment_size = 0;
            
            //calculate fragment data size
            if (curr_offset + MAX_CONTROL_DATA_LEN <= frame.len){
                fragment_size = curr_offset + MAX_CONTROL_DATA_LEN;
            } else {
                fragment_size = frame.len - curr_offset;
            }
            
            //create fragment
            res = create_generic_frame(frame.data, fragment_size, 
                make_generic_frame_from_header(frame.header), curr_offset, send_data, &frame_size);

            if (res != ESP_OK){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to create generic frame fragment");
                vPortFree(frame.data); //free entire data for now - will need to implement retries/sliding window soon
                return res; 
            }

            //need to schedule the next fragment
            if (curr_offset != MAX_CONTROL_DATA_LEN){
                frame.generic_frame_data_offset += fragment_size;
                frame.header.frag_info += 1; //increment frag_num
                res = push_frame_to_scheduler(frame, channel);
                if (res != ESP_OK){
                    ESP_LOGE(DEBUG_LINK_TAG, "Failed to schedule next generic frame fragment");
                    vPortFree(frame.data); //free entire data for now - will need to implement retries/sliding window soon
                    return res; 
                }
                
            } else {
                //Done fragmenting, can free data array
                vPortFree(frame.data);
            }

        } else {
            res = create_generic_frame(frame.data, frame.len, 
                make_generic_frame_from_header(frame.header), 0, send_data, &frame_size);
            vPortFree(frame.data);
        }

        if (res != ESP_OK){
            ESP_LOGE(DEBUG_LINK_TAG, "Failed to create generic frame");
            return res;
        }
    }

    uint8_t channel_to_route = MAX_CHANNELS;
    rmt_transmit_config_t config = {
        .loop_count = 0,
        .flags = {
            .eot_level = 0   // typically 0 or 1, depending on your output idle level
        }
    };
    if (frame.header.receiver_id == BROADCAST_ADDR){
        // printf("Sending on channel %d\n", i);
        phys_comms->send(send_data, frame_size, &config, channel);

    } else {
        res = route_frame(frame.header.receiver_id, &channel_to_route);

        if (res != ESP_OK){
            ESP_LOGE(DEBUG_LINK_TAG, "Failed to find entry for %d", frame.header.receiver_id);
            return ESP_FAIL;
        }
        // ESP_LOGI(DEBUG_LINK_TAG, "Sending message to %d", frame.header.receiver_id);
        phys_comms->send(send_data, frame_size, &config, channel_to_route);
    }

    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to send message");
        return ESP_FAIL;
    } else{
        // printf("Sent message to board %d\n", frame.header.receiver_id);
    }

    return ESP_OK;
}
