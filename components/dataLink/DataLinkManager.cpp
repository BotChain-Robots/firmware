#include "DataLinkManager.h"
#include "BlockingQueue.h"
#include "Frames.h"
#include "RMTManager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <memory>

#define SCHEDULE_QUEUE_SIZE 25

/**
 * @brief Constructs a new Data Link Manager object
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

    uint8_t existing_board_id = 0;
    get_board_id(existing_board_id);

    this_board_id = board_id;
    if (existing_board_id != board_id){
        set_board_id(this_board_id);
    }

    this->num_channels = num_channels;

    sequence_num_map_mutex = xSemaphoreCreateMutex();

    for (int i = 0; i < MAX_CHANNELS; i++) {
        frame_queue[i] = std::make_unique<BlockingPriorityQueue<SchedulerMetadata, std::vector<SchedulerMetadata>, FrameCompare>>(SCHEDULE_QUEUE_SIZE);
    }

    async_receive_queue = std::make_unique<BlockingQueue<Rx_Metadata>>(MAX_RX_QUEUE_SIZE);

    init_scheduler();
    init_rip();
}

/**
 * @brief Returns if the link layer is ready to receive frames
 *
 * @return esp_err_t
 */
esp_err_t DataLinkManager::ready(){
    return (phys_comms == nullptr || rip_broadcast_task == NULL || rip_ttl_task == NULL || scheduler_task == NULL || receive_task == NULL) ? ESP_FAIL : ESP_OK;
}

/**
 * @brief Atomic function to get and post increment sequence number map
 *
 * @param board_id
 * @param seq_num
 * @return esp_err_t
 */
esp_err_t DataLinkManager::get_inc_sequence_num(uint8_t board_id, uint16_t* seq_num){
    if (seq_num == NULL){
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(sequence_num_map_mutex, pdMS_TO_TICKS(SEQUENCE_NUM_MAP_MUTEX_MAX_WAIT_MS)) != pdTRUE){
        return ESP_FAIL;
    }

    *seq_num = sequence_num_map[board_id]++;

    xSemaphoreGive(sequence_num_map_mutex);

    return ESP_OK;
}

/**
 * @brief Atomic function to get sequence number map
 *
 * @param board_id
 * @param seq_num
 * @return esp_err_t
 */
esp_err_t DataLinkManager::get_sequence_num(uint8_t board_id, uint16_t* seq_num){
    if (seq_num == NULL){
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(sequence_num_map_mutex, pdMS_TO_TICKS(SEQUENCE_NUM_MAP_MUTEX_MAX_WAIT_MS)) != pdTRUE){
        return ESP_FAIL;
    }

    *seq_num = sequence_num_map[board_id];

    xSemaphoreGive(sequence_num_map_mutex);

    return ESP_OK;
}

DataLinkManager::~DataLinkManager(){
    stop_tasks = true;

    bool dummy = true;
    xQueueSend(manual_broadcasts, &dummy, 0);

    vTaskDelay(pdMS_TO_TICKS(100)); //delay to allow tasks to be killed

    if (rip_broadcast_task == NULL){
        vTaskDelete(rip_broadcast_task);
        rip_broadcast_task = NULL;
    }
    if (rip_ttl_task == NULL){
        vTaskDelete(rip_ttl_task);
        rip_ttl_task = NULL;
    }
    if (scheduler_task == NULL){
        vTaskDelete(scheduler_task);
        scheduler_task = NULL;
    }
    if (receive_task == NULL){
        vTaskDelete(receive_task);
        receive_task = NULL;
    }
    if (send_ack_task == NULL){
        vTaskDelete(send_ack_task);
        send_ack_task = NULL;
    }
}

esp_err_t DataLinkManager::set_board_id(uint8_t board_id){
    if (board_id == BROADCAST_ADDR || board_id == PC_ADDR){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid board id");
        return ESP_FAIL;
    }

    nvs_handle_t handle;
    esp_err_t res = nvs_open(NVS_BOARD_NAMESPACE, NVS_READWRITE, &handle);
    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to open NVS Handle");
        return res;
    }

    res = nvs_set_u8(handle, NVS_BOARD_ID_KEY, board_id);
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
    ESP_LOGI(DEBUG_LINK_TAG, "Successfully wrote %d to NVM", board_id);

    nvs_close(handle);

    return ESP_OK;
}

esp_err_t DataLinkManager::get_board_id(uint8_t& board_id){
    nvs_handle_t handle;
    esp_err_t res = nvs_open(NVS_BOARD_NAMESPACE, NVS_READWRITE, &handle);
    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to open NVS Handle");
        return res;
    }

    res = nvs_get_u8(handle, NVS_BOARD_ID_KEY, &board_id);
    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to get ID from NVM. Please make sure NVM is already assigned a board id!");
        nvs_close(handle);
        return res;
    }

    ESP_LOGI(DEBUG_LINK_TAG, "Successfully got board id %d from NVM", board_id);

    nvs_close(handle);

    return ESP_OK;
}

/**
 * @brief Helper function to create a control frame
 *
 * @param dest_board
 * @param data
 * @param data_len
 * @param type
 * @param flag
 * @return esp_err_t
 */
esp_err_t DataLinkManager::create_control_frame(uint8_t* data, uint16_t data_len, ControlFrame control_frame, uint8_t* send_data, size_t* send_data_len){
    if (data == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Data array does not exist");
        return ESP_ERR_INVALID_ARG;
    }

    if (this_board_id == PC_ADDR){
        ESP_LOGE(DEBUG_LINK_TAG, "This board is not assigned a board id");
        return ESP_ERR_INVALID_ARG;
    }

    if (data_len > MAX_FRAME_SIZE){
        ESP_LOGE(DEBUG_LINK_TAG, "Data for control frame is too large. Maximum size is %d. Current data length is %d", MAX_FRAME_SIZE, data_len);
        return ESP_ERR_INVALID_ARG;
    }

    if (send_data == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid pointer for send_data");
        return ESP_ERR_INVALID_ARG;
    }

    if (send_data_len == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid pointer for send_data_len");
        return ESP_ERR_INVALID_ARG;
    }

    if (*send_data_len < sizeof(ControlFrame)){
        ESP_LOGE(DEBUG_LINK_TAG, "Send data array is too small");
        return ESP_ERR_INVALID_ARG;
    }

    if (!IS_CONTROL_FRAME(control_frame.type_flag)){
        ESP_LOGE(DEBUG_LINK_TAG, "Must be a control frame type");
        return ESP_ERR_INVALID_ARG;
    }

    size_t offset = 0;
    send_data[offset++] = control_frame.preamble;
    send_data[offset++] = control_frame.sender_id;
    send_data[offset++] = control_frame.receiver_id;
    send_data[offset++] = control_frame.seq_num & 0xFF;
    send_data[offset++] = (control_frame.seq_num >> 8) & 0xFF;
    send_data[offset++] = control_frame.type_flag;
    send_data[offset++] = data_len;
    send_data[offset++] = (data_len >> 8) & 0xFF;

    memcpy(&send_data[offset], data, data_len);

    offset += control_frame.data_len;

    geneate_crc_16(send_data, offset, &control_frame.crc_16);

    send_data[offset++] = control_frame.crc_16 & 0xFF;
    send_data[offset++] = (control_frame.crc_16 >> 8) & 0xFF;

    *send_data_len = offset;

    // printf("Sending Frame Information:\n");
    // printf("%-10s %-12s %-13s %-15s %-12s %-10s %-6s\n",
    // "Preamble", "Sender ID", "Receiver ID", "Sequence Num", "Type+Flag", "Data Len", "CRC");

    // printf("0x%02X       %-12d %-13d %-15d  0x%02X       %-10d   0x%04X\n",
    // control_frame.preamble, control_frame.sender_id, control_frame.receiver_id, control_frame.seq_num, control_frame.type_flag, control_frame.data_len, control_frame.crc_16);

    return ESP_OK;
}

/**
 * @brief Helper function to create a generic frame
 *
 * @param data
 * @param data_len
 * @param generic_frame
 * @param offset
 * @param send_data
 * @param send_data_len
 * @return esp_err_t
 */
esp_err_t DataLinkManager::create_generic_frame(uint8_t* data, uint16_t data_len, GenericFrame generic_frame, uint16_t offset, uint8_t* send_data, size_t* send_data_len){
        if (data == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Data array does not exist");
        return ESP_ERR_INVALID_ARG;
    }

    if (this_board_id == PC_ADDR){
        ESP_LOGE(DEBUG_LINK_TAG, "This board is not assigned a board id");
        return ESP_ERR_INVALID_ARG;
    }

    if (data_len > MAX_FRAME_SIZE){
        ESP_LOGE(DEBUG_LINK_TAG, "Data for generic frame is too large. Maximum size is %d. Current data length is %d", MAX_FRAME_SIZE, data_len);
        return ESP_ERR_INVALID_ARG;
    }

    if (send_data == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid pointer for send_data");
        return ESP_ERR_INVALID_ARG;
    }

    if (send_data_len == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid pointer for send_data_len");
        return ESP_ERR_INVALID_ARG;
    }

    if (*send_data_len < sizeof(GenericFrame)){
        ESP_LOGE(DEBUG_LINK_TAG, "Send data array is too small");
        return ESP_ERR_INVALID_ARG;
    }

    if (IS_CONTROL_FRAME(generic_frame.type_flag)){
        ESP_LOGE(DEBUG_LINK_TAG, "Must be a generic frame type");
        return ESP_ERR_INVALID_ARG;
    }

    size_t send_data_offset = 0;
    send_data[send_data_offset++] = generic_frame.preamble;
    send_data[send_data_offset++] = generic_frame.sender_id;
    send_data[send_data_offset++] = generic_frame.receiver_id;
    send_data[send_data_offset++] = generic_frame.seq_num & 0xFF;
    send_data[send_data_offset++] = (generic_frame.seq_num >> 8) & 0xFF;

    send_data[send_data_offset++] = generic_frame.type_flag;

    send_data[send_data_offset++] = generic_frame.total_frag & 0xFF;
    send_data[send_data_offset++] = (generic_frame.total_frag >> 8) & 0xFF;

    send_data[send_data_offset++] = generic_frame.frag_num & 0xFF;
    send_data[send_data_offset++] = (generic_frame.frag_num >> 8) & 0xFF;

    send_data[send_data_offset++] = data_len;
    send_data[send_data_offset++] = (data_len >> 8) & 0xFF;

    memcpy(&send_data[send_data_offset], &data[offset], data_len);

    send_data_offset += data_len;

    geneate_crc_16(send_data, send_data_offset, &generic_frame.crc_16);

    send_data[send_data_offset++] = generic_frame.crc_16 & 0xFF;
    send_data[send_data_offset++] = (generic_frame.crc_16 >> 8) & 0xFF;

    *send_data_len = send_data_offset;

    // printf("Sending Frame Information:\n");
    // printf("%-10s %-12s %-13s %-15s %-12s %-10s %-6s\n",
    // "Preamble", "Sender ID", "Receiver ID", "Sequence Num", "Type+Flag", "Data Len", "CRC");

    // printf("0x%02X       %-12d %-13d %-15d  0x%02X       %-10d   0x%04X\n",
    // generic_frame.preamble, generic_frame.sender_id, generic_frame.receiver_id, generic_frame.seq_num, generic_frame.type_flag, generic_frame.data_len, generic_frame.crc_16);

    return ESP_OK;
}

/**
 * @brief Schedules a frame to be sent via RMT
 *
 * @param dest_board 8 bit ID of the destination board
 * @param data
 * @param data_len Length of the data in bytes
 * @param type
 * @return esp_err_t
 */
esp_err_t DataLinkManager::send(uint8_t dest_board, std::unique_ptr<std::vector<uint8_t>>&& buffer, FrameType type, uint8_t flag){
    bool isControlFrame = IS_CONTROL_FRAME((uint8_t)type);

    if (isControlFrame && buffer->size() > MAX_FRAME_SIZE){
        //Control frames has max data size of MAX_FRAME_SIZE
        return ESP_ERR_INVALID_ARG;
    }

    if (!isControlFrame && buffer->size() > MAX_GENERIC_NUM_FRAG * MAX_GENERIC_DATA_LEN){
        //Generic frames has max MAX_GENERIC_NUM_FRAG fragments, each max size of MAX_GENERIC_DATA_LEN (data size)
        return ESP_ERR_INVALID_ARG;
    }

    if (!isControlFrame && dest_board == BROADCAST_ADDR && type != FrameType::MISC_UDP_GENERIC_TYPE){
        //If broadcasting generic frames, we don't to spam acks to this board
        return ESP_ERR_INVALID_ARG;
    }

    //calculate number of fragments required (for generic frames only)
    uint32_t frag_info = 0;
    if (!isControlFrame){
        if (buffer->size() <= MAX_CONTROL_DATA_LEN){
            frag_info = (1 << 16); //1 total fragment required
        } else {
            uint32_t total_frags = (buffer->size() + MAX_GENERIC_DATA_LEN - 1) / MAX_GENERIC_DATA_LEN;
            frag_info = (total_frags) << 16;
        }
    }

    uint16_t seq_num = 0;

    esp_err_t res = get_inc_sequence_num(dest_board, &seq_num);
    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed atomic get increment sequence number map");
        return res;
    }

    SchedulerMetadata metadata = {
        .header = {
            .preamble = START_OF_FRAME,
            .sender_id = this_board_id,
            .receiver_id = dest_board,
            .seq_num = seq_num,
            .type_flag = (uint8_t)((static_cast<uint8_t>(type) & 0xF0) | (flag & 0xF)),
            .frag_info = frag_info,
            .data_len = (uint16_t)buffer->size(),
            .crc_16 = 0,
        },
        .generic_frame_data_offset = 0,
        .enqueue_time_ns = 0,
        .data = std::move(buffer),
        .last_ack = 0,
        .curr_fragment = 0,
        .timeout = 0,
    };

    uint8_t channel = 0;
    res = route_frame(dest_board, &channel);

    if (res != ESP_OK){
        // ESP_LOGE(DEBUG_LINK_TAG, "Failed to route message to board %d", dest_board);
        return res;
    }

    res = push_frame_to_scheduler(metadata, channel);

    if (res != ESP_OK){
        ESP_LOGE(DEBUG_LINK_TAG, "Failed to push frame to scheduler queue");
    }
    return res;
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
 * @deprecated This function is deprecated. This is replaced by `async_receive_info` and `async_receive`. This function returns `ESP_FAIL`
 *
 * @brief Starts the RMT async receive job to start listening for a new frame over a given channel
 *
 * @param curr_channel
 * @return esp_err_t
 */
esp_err_t DataLinkManager::start_receive_frames(uint8_t curr_channel){
    return ESP_FAIL;
}

/**
 * @brief Starts the RMT async receive job to start listening for a new frame over a given channel
 *
 * @param curr_channel
 * @return esp_err_t
 */
esp_err_t DataLinkManager::start_receive_frames_rmt(uint8_t curr_channel){
    if (curr_channel >= num_channels){
        return ESP_FAIL;
    }
    return phys_comms->start_receiving(curr_channel);
}

/**
 * @deprecated This function is deprecated. This is replaced by `async_receive_info` and `async_receive`. This function returns `ESP_FAIL`
 *
 * @brief Receive Control Frame from RMT Physical Layer
 *
 * @param data Byte array
 * @param data_len Length of the byte array
 * @param recv_len Length of the received data
 * @param curr_channel Physical channel pair to look at
 * @return esp_err_t
 */
esp_err_t DataLinkManager::receive(uint8_t* data, size_t data_len, size_t* recv_len, uint8_t curr_channel){
    return ESP_FAIL;
}

/**
 * @brief
 *
 * @param data
 * @param data_len
 * @param message
 * @param message_size
 * @param header
 * @return esp_err_t
 *
 * @deprecated
 * Will be moved to private function
 */
esp_err_t DataLinkManager::get_data_from_frame(uint8_t* data, size_t data_len, uint8_t* message, size_t* message_size, FrameHeader* header){
    if (data == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid data array");
        return ESP_ERR_INVALID_ARG;
    }
    if (message == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid message array");
        return ESP_ERR_INVALID_ARG;
    }
    if (message_size == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid message size ptr");
        return ESP_ERR_INVALID_ARG;
    }
    if (header == nullptr){
        ESP_LOGE(DEBUG_LINK_TAG, "Invalid header ptr");
        return ESP_ERR_INVALID_ARG;
    }

    header->preamble = data[0];
    header->sender_id = data[1];
    header->receiver_id = data[2];
    header->seq_num = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
    header->type_flag = data[5];
    if (IS_CONTROL_FRAME(data[5])){
        if (data_len < 9){
            return ESP_ERR_INVALID_SIZE;
        }

        header->data_len = (uint16_t)data[6] | ((uint16_t)data[7] << 8);

        if (header->data_len > data_len){
            ESP_LOGE(DEBUG_LINK_TAG, "Mismatch data length in control frame");
            return ESP_ERR_INVALID_RESPONSE;
        }

        if (header->data_len == 0){
            ESP_LOGE(DEBUG_LINK_TAG, "Data len 0");
            return ESP_ERR_INVALID_SIZE;
        }

        *message_size = header->data_len;

        if (*message_size > MAX_CONTROL_DATA_LEN || (10 + *message_size > data_len)){
            ESP_LOGE(DEBUG_LINK_TAG, "Invalid payload length: %u", *message_size);
            return ESP_ERR_INVALID_SIZE;
        }

        memcpy(message, &data[8], header->data_len);

        geneate_crc_16(data, 8*sizeof(uint8_t) + header->data_len, &header->crc_16);

        uint16_t crc_calc = ((uint16_t)data[8 + header->data_len] | ((uint16_t)data[9 + header->data_len] << 8));

        if (crc_calc != header->crc_16){
            //CRC mismatch
            ESP_LOGE(DEBUG_LINK_TAG, "CRC Mismatch - Control Frame");
            ESP_LOGE(DEBUG_LINK_TAG, "Got 0x%04X but calculated 0x%04X\n", crc_calc, header->crc_16);
            return ESP_ERR_INVALID_CRC;
        }

    } else {
        //generic frame

        if (data_len < 13){
            return ESP_ERR_INVALID_SIZE;
        }

        uint16_t total_frag = (uint16_t)data[6] | ((uint16_t)data[7] << 8);
        uint16_t frag_num = (uint16_t)data[8] | ((uint16_t)data[9] << 8);
        header->frag_info = (total_frag << 16) | (frag_num);
        header->data_len = (uint16_t)data[10] | ((uint16_t)data[11] << 8);

        *message_size = header->data_len;

        if ((*message_size > MAX_GENERIC_DATA_LEN && total_frag != 1) || (14 + *message_size > data_len)){
            ESP_LOGE(DEBUG_LINK_TAG, "Invalid payload length: %u", *message_size);
            return ESP_ERR_INVALID_SIZE;
        }

        memcpy(message, &data[12], *message_size);

        if (total_frag != 1){
            geneate_crc_16(data, 12*sizeof(uint8_t) + *message_size, &header->crc_16);
        } else {
            header->crc_16 = 0;
        }

        uint16_t crc_calc = ((uint16_t)data[12 + *message_size] | ((uint16_t)data[13 + *message_size] << 8));

        if (crc_calc != header->crc_16 && total_frag != 1){
            //CRC mismatch
            ESP_LOGE(DEBUG_LINK_TAG, "CRC Mismatch - Generic Frame");
            ESP_LOGE(DEBUG_LINK_TAG, "Got 0x%04X but calculated 0x%04X\n", crc_calc, header->crc_16);
            return ESP_ERR_INVALID_CRC;
        }
    }

    // printf("Received Frame Information:\n");
    // printf("%-10s %-12s %-13s %-15s %-12s %-10s %-6s\n",
    // "Preamble", "Sender ID", "Receiver ID", "Sequence Num", "Type+Flag", "Data Len", "CRC");

    // printf("0x%02X       %-12d %-13d %-15d  0x%02X       %-10d   0x%04X\n",
    // header->preamble, header->sender_id, header->receiver_id, header->seq_num, header->type_flag, header->data_len, header->crc_16);

    // printf("Message received: %.*s\n", *message_size, message);

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

/**
 * @brief Prints to console the encoded frame information from a byte array recevied from RMT
 *
 * @note Should only be used for debug purposes
 *
 * @warning This function may not be reliable/buggy
 *
 * @param data
 * @param data_len
 * @param message
 * @param message_len
 * @return esp_err_t
 */
esp_err_t DataLinkManager::print_frame_info(uint8_t* data, size_t data_len, uint8_t* message, size_t message_len){
    // printf("Received frame of size %d:\n", data_len);

    FrameHeader temp;

    // print_buffer_binary(data, data_len);
    return get_data_from_frame(data, data_len, message, &message_len, &temp);
}
