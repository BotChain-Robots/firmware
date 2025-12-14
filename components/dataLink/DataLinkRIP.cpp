#include "DataLinkManager.h"
#include "esp_log.h"
#include "freertos/semphr.h"

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

    
    // ESP_LOGI(DEBUG_LINK_TAG, "board_id %d now has hops %d from channel %d", (*entry)->info.board_id, (*entry)->info.hops, channel);
    
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

    if ((*entry)->info.hops >= new_hop && (*entry)->info.hops != RIP_MAX_HOPS + 1){ //no count to infinity if path is invalid
        (*entry)->info.hops = new_hop;
        (*entry)->channel = channel;
        // ESP_LOGI(DEBUG_LINK_TAG, "updated board_id %d now has hops %d from channel %d", (*entry)->info.board_id, (*entry)->info.hops, channel);
    }
    
    (*entry)->ttl = RIP_TTL_START;
    (*entry)->valid = 1; 

    // ESP_LOGI(DEBUG_LINK_TAG, "refreshed board_id %d ttl", (*entry)->info.board_id);

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
            // ESP_LOGI(DEBUG_LINK_TAG, "Found %d in table at row %d", board_id, i);
            return ESP_OK;
        } 
        if (rip_table[i].valid == RIP_INVALID_ROW && free_slot == nullptr){
            free_slot = &rip_table[i];
        }
        xSemaphoreGive(rip_table[i].row_sem);
    }

    // ESP_LOGI(DEBUG_LINK_TAG, "Finished looking for %d in table", board_id);

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
        // ESP_LOGI(DEBUG_LINK_TAG, "Reserved new entry for board %d", board_id);
    }

    return ESP_OK;
}

/**
 * @brief Returns the associated RIP Table row by row number. Information returned is read only.
 * 
 * @param entry 
 * @param row_num 
 * @return esp_err_t 
 */
esp_err_t DataLinkManager::rip_get_row(RIPRow** entry, uint8_t row_num){
    if (entry == nullptr){
        return ESP_ERR_INVALID_ARG;
    }

    if (row_num >= RIP_MAX_ROUTES){
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(rip_table[row_num].row_sem, (TickType_t)RIP_MAX_SEM_WAIT) != pdTRUE) return ESP_ERR_TIMEOUT;

    if (rip_table[row_num].valid == RIP_INVALID_ROW){
        xSemaphoreGive(rip_table[row_num].row_sem);
        return ESP_ERR_INVALID_STATE;
    }

    if (rip_table[row_num].info.hops == RIP_MAX_HOPS + 1){
        //invalid hop, decrement counter
        rip_table[row_num].ttl_flush--;
        if (rip_table[row_num].ttl_flush == 0){
            rip_table[row_num].valid = RIP_INVALID_ROW;
            xSemaphoreGive(rip_table[row_num].row_sem);
            return ESP_FAIL;
        }
    }

    *entry = &rip_table[row_num];

    xSemaphoreGive(rip_table[row_num].row_sem);

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
    uint16_t message_idx = 0;

    esp_err_t res;

    RIPRow* entry = nullptr;

    if(broadcast){
        for (size_t channel = 0; channel < num_channels; channel++){
            for (size_t i = 0; i < RIP_MAX_ROUTES; i++){
                res = rip_get_row(&entry, i);
                
                if (res != ESP_OK){
                    continue;
                }
                
                if (entry == nullptr){
                    continue;
                }

                // ESP_LOGI(DEBUG_LINK_TAG, "Found entry for board %d with hops %d", entry->info.board_id, entry->info.hops);
                
                if (entry->channel == channel){
                    //poisoned reverse
                    rip_message[message_idx++] = entry->info.board_id;
                    rip_message[message_idx++] = RIP_MAX_HOPS + 1;
                } else {
                    rip_message[message_idx++] = entry->info.board_id;
                    rip_message[message_idx++] = entry->info.hops;
                }
            }

            uint8_t* send_data = (uint8_t*)pvPortMalloc(message_idx);
            if (send_data == nullptr){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to malloc when trying to send rip frame broadcast on channel %d", channel);
                continue;
            }
            memset(send_data, 0, message_idx);
            memcpy(send_data, rip_message, message_idx);
            
            SchedulerMetadata metadata = {
                .header = {
                    .preamble = START_OF_FRAME,
                    .sender_id = this_board_id,
                    .receiver_id = BROADCAST_ADDR,
                    .seq_num = sequence_num_map[BROADCAST_ADDR]++,
                    .type_flag = static_cast<uint8_t>(FrameType::RIP_TABLE_CONTROL),
                    .data_len = message_idx,
                    .crc_16 = 0,
                },
                .generic_frame_data_offset = 0,
                .enqueue_time_ns = 0,
                .data = send_data,
                .len = message_idx,
            };

            res = push_frame_to_scheduler(metadata, channel);
            if (res != ESP_OK){
                ESP_LOGE(DEBUG_LINK_TAG, "Failed to schedule rip frame from send_rip_frame for channel %d", channel);
            }
            message_idx = 0;
            memset(rip_message, 0, sizeof(rip_message));
        }
    } else {
        for (size_t i = 0; i < RIP_MAX_ROUTES; i++){
            res = rip_get_row(&entry, i);

            if (res != ESP_OK){
                continue;
            }

            if (entry == nullptr){
                continue;
            }
            rip_message[message_idx++] = entry->info.board_id;
            rip_message[message_idx++] = entry->info.hops;
        }
        ESP_LOGI(DEBUG_LINK_TAG, "replying to discovery request to board %d", dest_id);
        res = send(dest_id, rip_message, message_idx, FrameType::RIP_TABLE_CONTROL, FLAG_DISCOVERY);
        if (res != ESP_OK){
            ESP_LOGE(DEBUG_LINK_TAG, "Failed to send rip frame from send_rip_frame");
        }
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
        return ESP_ERR_NOT_FOUND;
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

[[noreturn]] void DataLinkManager::rip_broadcast_timer_function(void* args){
    DataLinkManager* link_layer_obj = static_cast<DataLinkManager*>(args);
    if (link_layer_obj == nullptr || link_layer_obj->manual_broadcasts == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "RIP Broadacst task failed to start due to invalid pointer");
        vTaskDelete(nullptr);
    }

    ESP_LOGI(DEBUG_LINK_TAG, "Starting RIP broadcast task");

    esp_err_t res;
    while(!link_layer_obj->stop_tasks){
        bool dummy;
        xQueueReceive(link_layer_obj->manual_broadcasts, &dummy, pdMS_TO_TICKS(RIP_BROADCAST_INTERVAL)); //wait up to RIP_BROADCAST_INTERVAL ms
        ESP_LOGI(DEBUG_LINK_TAG, "Broadcasting table..."); //debug
        res = link_layer_obj->send_rip_frame(true, 0);
        if (res != ESP_OK){
            ESP_LOGE(DEBUG_LINK_TAG, "Failed to broadcast rip frame");
        }
    }
    vTaskDelete(nullptr);
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
    while(!link_layer_obj->stop_tasks){
        vTaskDelay(pdMS_TO_TICKS(RIP_MS_TO_SEC)); //run every second
        for (size_t i = 1; i < RIP_MAX_ROUTES; i++){
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
    vTaskDelete(nullptr);
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
    xTaskCreate(DataLinkManager::rip_broadcast_timer_function, "RIPBroadcast", 4096, static_cast<void*>(this), 5, &rip_broadcast_task);
    ESP_LOGI(DEBUG_LINK_TAG, "Starting RIP TTL task");
    xTaskCreate(DataLinkManager::rip_ttl_decrement_task, "RIPTTL", 2048, static_cast<void*>(this), 5, &rip_ttl_task);
} 