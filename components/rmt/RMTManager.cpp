#include "RMTManager.h"

#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

RMTManager::RMTManager(){
    esp_err_t res = init();
    if (res != ESP_OK){
        //failed
        ESP_LOGE(DEBUG_TAG, "Failed to initialize the RMTManager");
        return;
    }
    ESP_LOGD(DEBUG_TAG, "RMTManager has been initialized");
}

esp_err_t RMTManager::init_tx_channel(){
    esp_err_t res_tx = ESP_FAIL;

    //setup encoder config
    
    for (uint8_t i = 0; i < MAX_CHANNELS; i++){
        reset_encoder_context(&channels[i].encoder_context); //ensure the encoder context is initialized
        rmt_simple_encoder_config_t encoder_config = {
            .callback = encoder_callback,
            .arg = &channels[i].encoder_context
        };
        
        //create encoder
        res_tx = rmt_new_simple_encoder(&encoder_config, &channels[i].encoder);
    
        if (res_tx != ESP_OK){
            // printf("Failed to create encoder\n");
            ESP_LOGE(DEBUG_TAG, "Failed to create encoder");
            channels[i].encoder = NULL;
            return ESP_FAIL;
        }
    
        //enable the callback
        rmt_tx_event_callbacks_t tx_cbs = {
            .on_trans_done = RMTManager::rmt_tx_done_callback
        };

        rmt_tx_channel_config_t tx_channel_config_template = {
            .gpio_num = tx_gpio[i],
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = RMT_RESOLUTION_HZ,
            .mem_block_symbols = RMT_SYMBOL_BLOCK_SIZE, //giving each channel ~192B of memory
            .trans_queue_depth = 4,
            .flags = {
                .invert_out = 0,
                .with_dma = 0,
            }
        }; 
        
        channels[i].tx_gpio = tx_gpio[i];
        channels[i].status = CHANNEL_NOT_READY_STATUS;

         if (channels[i].tx_rmt_handle != NULL) {
            rmt_disable(channels[i].tx_rmt_handle);
            rmt_del_channel(channels[i].tx_rmt_handle);
            channels[i].tx_rmt_handle = NULL;
        }
        if (channels[i].tx_done_semaphore != NULL) {
            vSemaphoreDelete(channels[i].tx_done_semaphore);
            channels[i].tx_done_semaphore = NULL;
        }

        channels[i].tx_queue = xQueueCreate(QUEUE_SIZE, sizeof(TxBuffer)); //can store up to 10 queued transmissions (each transmission size being 192B; based ont he RMT_SYMBOL_BLOCK_SIZE)

        res_tx = rmt_new_tx_channel(&tx_channel_config_template, &channels[i].tx_rmt_handle);
        
        //init tx channel
        if (res_tx != ESP_OK) {
            // printf("Failed to init TX channel\n");
            ESP_LOGE(DEBUG_TAG, "Failed to init TX channel %d", i);
            continue;
        }
    
        if (channels[i].tx_rmt_handle == NULL) {
            // printf("TX channel handle is NULL\n");
            ESP_LOGE(DEBUG_TAG, "TX channel handle is NULL on channel %d", i);
            continue;
        }

        
        channels[i].tx_done_semaphore = xSemaphoreCreateBinary(); //create a binary sem
        
        TxCallbackContext* tx_callback_ctx = new TxCallbackContext {
            .tx_done_sem = channels[i].tx_done_semaphore,
            .transmit_queue = channels[i].tx_queue,
            .tx_context = &channels[i].encoder_context
        };

        if (channels[i].tx_done_semaphore == NULL){
            ESP_LOGE(DEBUG_TAG, "Failed to create TX done semaphore on channel %d", i);
            continue;
        }

        // res_tx = rmt_tx_register_event_callbacks(channels[i].tx_rmt_handle, &tx_cbs, channels[i].tx_done_semaphore);
        res_tx = rmt_tx_register_event_callbacks(channels[i].tx_rmt_handle, &tx_cbs, static_cast<void*>(tx_callback_ctx));

        if (res_tx != ESP_OK) {
            // printf("Failed to register TX callback\n");
            ESP_LOGE(DEBUG_TAG, "Failed to register TX callback on channel %d", i);
            continue;
        }

        //enable tx channels
        res_tx = rmt_enable(channels[i].tx_rmt_handle);

        if (res_tx != ESP_OK) {
            // printf("Failed to enable TX channel\n");
            ESP_LOGE(DEBUG_TAG, "Failed to enable TX channel %d", i);
            continue;
        }

        printf("Successfully enabled TX channel %d\n", i);
    }

    return ESP_OK;
}

bool RMTManager::rmt_tx_done_callback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_data){
    BaseType_t high_task_wakeup = pdFALSE;

    // SemaphoreHandle_t sem = (SemaphoreHandle_t)user_data;

    TxCallbackContext* args = static_cast<TxCallbackContext*>(user_data);

    SemaphoreHandle_t sem = args->tx_done_sem;
    QueueHandle_t queue = args->transmit_queue;
    rmt_encoder_context_t* encoder_context = args->tx_context;

    
    TxBuffer buf = {};
    BaseType_t xTaskWokenByReceive = pdFALSE;
    xQueueReceiveFromISR(queue, static_cast<TxBuffer*>(&buf), &xTaskWokenByReceive); //remove from the queue
   
    if (buf.data != nullptr){
        vPortFree((void*)buf.data);
    }

    if (encoder_context != nullptr){
        encoder_context->bit_index = 0;
        encoder_context->byte_index = 0;
        encoder_context->num_symbols = 0;
    }
    
    xSemaphoreGiveFromISR(sem, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

esp_err_t RMTManager::wait_until_send_complete(uint8_t channel_num){
    if (channel_num >= MAX_CHANNELS){
        ESP_LOGE(DEBUG_TAG, "Invalid channel number");
        return ESP_FAIL;
    }

    if(this->channels[channel_num].tx_done_semaphore == NULL){
        return ESP_FAIL;
    }

    if (xSemaphoreTake(this->channels[channel_num].tx_done_semaphore, pdMS_TO_TICKS(10000)) == pdTRUE){
        return ESP_OK;
    }

    ESP_LOGE(DEBUG_TAG, "Timeout of 10000 ms when waiting for RMT TX to complete");
    return ESP_FAIL;
}

bool RMTManager::rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data){
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;
    // send the received RMT symbols to the parser task
    BaseType_t res = xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    if (res != pdTRUE){
        // printf("RX Callback: Failed to enqueue received data\n");
        ESP_LOGE(DEBUG_TAG, "RX Callback: Failed to enqueue received data");
    }
    // return whether any task is woken up
    return high_task_wakeup == pdTRUE;
}

esp_err_t RMTManager::init_rx_channel(){
    for (uint8_t i = 0; i < MAX_CHANNELS; i++){
        rmt_rx_channel_config_t rx_channel_config = {
            .gpio_num = rx_gpio[i],
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = RMT_RESOLUTION_HZ,
            .mem_block_symbols = RMT_SYMBOL_BLOCK_SIZE,
            .flags = {
                .invert_in = false,
                .with_dma = 0
            }
        }; //temp for one rx channel
    
        //temp
        channels[i].rx_gpio = rx_gpio[i];
        
        esp_err_t res_rx = rmt_new_rx_channel(&rx_channel_config, &channels[i].rx_rmt_handle);
    
        if (res_rx != ESP_OK) {
            // printf("Failed to init RX channel - reason %s\n", esp_err_to_name(res_rx));
            ESP_LOGE(DEBUG_TAG, "Failed to init RX channel - reason %s", esp_err_to_name(res_rx));
            return ESP_FAIL;
        }
    
        if (channels[i].rx_rmt_handle == NULL) {
            // printf("RX channel handle is NULL\n");
            ESP_LOGE(DEBUG_TAG, "RX channel handle is NULL");
            return ESP_FAIL;
        }
    
        channels[i].rx_queue = xQueueCreate(QUEUE_SIZE, sizeof(rmt_rx_done_event_data_t)); //creating queue with some random size
    
        rmt_rx_event_callbacks_t cbs = {
            .on_recv_done = RMTManager::rmt_rx_done_callback
        };
        rmt_rx_register_event_callbacks(channels[i].rx_rmt_handle, &cbs, channels[i].rx_queue);
    
        res_rx = rmt_enable(channels[i].rx_rmt_handle);
    
        if (res_rx != ESP_OK) {
            // printf("Failed to enable RX channel\n");
            ESP_LOGE(DEBUG_TAG, "Failed to enable RX channel");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t RMTManager::init(){
    esp_err_t res = this->init_tx_channel();
    if (res != ESP_OK) {
        // printf("Failed to init TX channel\n");
        ESP_LOGE(DEBUG_TAG, "Failed to init TX channel");
        return ESP_FAIL;
    }
    
    res = this->init_rx_channel();
    if (res != ESP_OK) {
        // printf("Failed to init RX channel\n");
        ESP_LOGE(DEBUG_TAG, "Failed to init RX channel");

        return ESP_FAIL;
    }

    for (uint8_t i = 0; i < MAX_CHANNELS; i++){
        if (channels[i].tx_rmt_handle != NULL && channels[i].rx_rmt_handle != NULL && channels[i].tx_done_semaphore != NULL && channels[i].rx_queue != NULL){
            channels[i].status = CHANNEL_READY_STATUS;
        }
    }
    
    // printf("Free heap before encoder creation: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    // heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
    // printf("Free DMA-capable heap before encoder creation: %d bytes\n", heap_caps_get_free_size(MALLOC_CAP_DMA));
    // heap_caps_print_heap_info(MALLOC_CAP_DMA);
    return ESP_OK;
}

/**
 * @brief This is a callback function called by RMT when transmitting. This function will encode the user data `data` with rising and falling edges based on the bit.a64l
 * The symbols are defined in `RMTManager.h`, where a bit 1 is transmitted as a `RMT_SYMBOL_ONE` and a bit 0 is transmitted as a `RMT_SYMBOL_ZERO`
 * 
 * @param data
 * @param data_size
 * @param symbols_written
 * @param symbols_free
 * @param symbols
 * @param done
 * @param arg
 */
size_t RMTManager::encoder_callback(const void* data, size_t data_size, size_t symbols_written, 
            size_t symbols_free, rmt_symbol_word_t* symbols, bool* done, void* arg){      

    rmt_encoder_context_t* ctx = (rmt_encoder_context_t*) arg; //get the current context
    if (symbols_free == 0){ //no space in the tx buffer; don't encode any more bytes until there is space left
        *done = (ctx->byte_index >= data_size);
        return 0;
    }
    const uint8_t* bytes = (const uint8_t*)data; //get the user data as an array of bytes
    size_t symbols_used = 0; //number of symbols used
    while (ctx->byte_index < data_size && symbols_used < symbols_free){ //loop until we have reached the end of the data or filled the RMT symbol buffer (`symbols_free`)
        uint8_t byte = bytes[ctx->byte_index]; //get the byte from the data
        uint8_t bit = (byte >> (7 - ctx->bit_index)) & 0x01; //get the current bit, as determined from the bit index (MSB first)
        
        #ifndef NRZ_INVERTED
            //Manchester (Ethernet Standard) Encoding 
            symbols[symbols_used++] = bit ? RMT_SYMBOL_ONE : RMT_SYMBOL_ZERO; //if the bit is a 1, transmit a 1 symbol; otherwise, transmit 0 symbol
            ctx->num_symbols++;
        #else
            //NRZ-I encoding. Must change the voltage level whenever a bit 1 is detected
            if (ctx->byte_index == 0 && ctx->bit_index == 0){
                //MSB of the first byte - send a rising edge 1 to allow any succeeding 0s to be detected by the receiver
                symbols[symbols_used++] = RMT_SYMBOL_ONE_RISING; 
                ctx->current_level = !ctx->current_level; //current level is high
                ctx->num_symbols++;
            }

            if (ctx->zero_count == CONSEC_ZERO_THRESHOLD){
                ctx->current_level = !ctx->current_level;
                symbols[symbols_used++] = ctx->current_level ? RMT_SYMBOL_ONE_RISING : RMT_SYMBOL_ONE_FALLING;
                ctx->num_symbols++;
                ctx->zero_count = 0;

                // Don't advance to next bit – reprocess the current bit
                continue;
            }

            if (bit == 1){
                ctx->current_level = !ctx->current_level; //invert current level
                symbols[symbols_used++] = ctx->current_level ? RMT_SYMBOL_ONE_RISING : RMT_SYMBOL_ONE_FALLING; //if current level is 0 (low), it must be a falling edge. otherwise, it is a rising edge
                ctx->num_symbols++;
                ctx->zero_count = 0;
            } else {
                //bit 0s, maintain current level
                if (ctx->current_level){
                    //check if the previous symbol was RMT_SYMBOL_ZERO_HIGH. if it is, simply add another RMT_DURATION_MAX on duration1 (this is a slight optimization to send less symbols)
                    if (symbols[symbols_used-1].level0 == 1 && symbols[symbols_used-1].level1 == 1){
                        symbols[symbols_used-1].duration1 += RMT_DURATION_MAX;
                    } else {
                        //previous symbol was not RMT_SYMBOL_ZERO_HIGH
                        symbols[symbols_used++] = RMT_SYMBOL_ZERO_HIGH;
                        ctx->num_symbols++;
                    }
                } else {
                    if (symbols[symbols_used-1].level0 == 0 && symbols[symbols_used-1].level1 == 0){
                        symbols[symbols_used-1].duration1 += RMT_DURATION_MAX;
                    } else {
                        symbols[symbols_used++] = ctx->current_level ? RMT_SYMBOL_ZERO_HIGH : RMT_SYMBOL_ZERO_LOW;
                        ctx->num_symbols++;
                    }
                }

                ctx->zero_count++;

            }

        #endif //NRZ_INVERTED
        ctx->bit_index++;
        if (ctx->bit_index >= 8) { 
            //reached the end of the byte; go to the next byte
            ctx->bit_index = 0;
            ctx->byte_index++;
        }
    }

    *done = (ctx->byte_index >= data_size); //if the transmit is done, set the `done` flag to true (all bytes have been encoded)
    return symbols_used;
}

void RMTManager::reset_encoder_context(rmt_encoder_context_t* ctx){
    ctx->bit_index = 0;
    ctx->byte_index = 0;
    ctx->num_symbols = 0;
    #ifdef NRZ_INVERTED
    ctx->current_level = false;
    #endif //NRZ_INVERTED
}

/**
 * @brief Sends the string `data` of size `size`, with config `config`
 * 
 * @param data 
 * @param size 
 * @param config 
 * @return int 
 */
int RMTManager::send(uint8_t* data, size_t size, rmt_transmit_config_t* config, uint8_t channel_num){
    if (channel_num >= MAX_CHANNELS){
        ESP_LOGE(DEBUG_TAG, "send() error: invalid channel number");
    }

    if (channels[channel_num].status == CHANNEL_NOT_READY_STATUS){
        ESP_LOGE(DEBUG_TAG, "send() error: Channel %d is not ready", channel_num);
    }

    if (this->channels[channel_num].tx_rmt_handle == nullptr) {
        // printf("send() error: tx_chan is NULL\n");
        ESP_LOGE(DEBUG_TAG, "send() error: tx_chan is NULL");
        return ESP_FAIL;
    }
    if (this->channels[channel_num].encoder == nullptr) {
        // printf("send() error: encoder is NULL\n");
        ESP_LOGE(DEBUG_TAG, "send() error: encoder is NULL");
        return ESP_FAIL;
    }
    if (data == nullptr || size == 0 || size > (RMT_SYMBOL_BLOCK_SIZE*4)) {
        // printf("send() error: data pointer NULL or size 0\n");
        ESP_LOGE(DEBUG_TAG, "send() error: data pointer NULL or size 0");
        return ESP_FAIL;
    }
    if (config == nullptr) {
        // printf("send() error: config pointer is NULL\n");
        ESP_LOGE(DEBUG_TAG, "send() error: config pointer is NULL");
        return ESP_FAIL;
    }

    TxBuffer new_data_to_send_buf = {
        .data = (uint8_t*)pvPortMalloc(size), //this may not be thread safe but each channel should be on its own thread so maybe it's ok???
        .length = size
    };

    if (new_data_to_send_buf.data == nullptr){
        ESP_LOGE(DEBUG_TAG, "failed to malloc");
        return ESP_FAIL;
    }

    memcpy((void*)(new_data_to_send_buf.data), data, size);
    
    if (xQueueSendToBack(channels[channel_num].tx_queue, (void*)&new_data_to_send_buf, (TickType_t) 10) != pdPASS){ //note this may not work very well since im not checking the return value; this function can fail if the queue is full
        vPortFree((void*)new_data_to_send_buf.data);
        ESP_LOGE(DEBUG_TAG, "Failed to queue data");
        return ESP_FAIL;
    } 

    esp_err_t res = rmt_transmit(this->channels[channel_num].tx_rmt_handle, this->channels[channel_num].encoder, new_data_to_send_buf.data, new_data_to_send_buf.length, config);

    if (res != ESP_OK){
        // printf("Failed to send %s\n", data);
        vPortFree((void*)new_data_to_send_buf.data);
        ESP_LOGE(DEBUG_TAG, "Failed to send %s", data);
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief This function, given the `symbols` and the length `num`, will convert the received symbols into the symbols defined in `RMTSymbols.h`
 * this somehow works first try????? (tested with 't', 'O', and 'THIS IS A SAMPLE TEXT MESSAGE')
 * @param symbols received symbols 
 * @param num number of received symbols
 * @param decoded decoded symbol string
 * @param output_num size of `decoded`
 * @return int - returns the number of symbols written to the buffer
 */
int RMTManager::decode_symbols(rmt_symbol_word_t* symbols, size_t num, rmt_symbol_word_t* decoded, size_t output_num){
    if (symbols == NULL || decoded == NULL || num == 0 || output_num == 0){
        return ESP_FAIL;
    }

    size_t output_index = 0;
    size_t i = 0;
    bool curr_high_low = true; //flag to maintain where we are (either high or low)

    #ifdef NRZ_INVERTED
        uint32_t num_0_symbols_duration = 0, num_0_symbols = 0; 
        uint8_t consecutive_zeros = 0;
    #endif //NRZ_INVERTED
    while (output_index < output_num && i < num){
        // printf("duration0 %d level0 %d duration1 %d level1 %d\n", symbols[i].duration0, symbols[i].level0, symbols[i].duration1, symbols[i].level1); //dummy print receive
        #ifndef NRZ_INVERTED
            //manchester encoding
            /*there are two cases in the beginning:
            1. if duration0 = 20, then we are in between two symbols (low to high and high to low). 
            in this case, we need to insert a low in the beginning and "split" the current symbol into 2
            2. if duration0 = 10, then the first symbol should be high to low
            */
            if (symbols[i].duration0 != RMT_DURATION_SYMBOL){
                if (i != 0){
                    if (curr_high_low){
                        decoded[output_index++] = RMT_SYMBOL_ONE;
                    } else {
                        decoded[output_index++] = RMT_SYMBOL_ZERO;
                    }
                    curr_high_low = !curr_high_low;
                } else {
                    //need to insert a 0 before received symbols
                    decoded[output_index++] = RMT_SYMBOL_ZERO;
                }
            
            }
            
            if (curr_high_low){
                decoded[output_index++] = RMT_SYMBOL_ONE;
            } else {
                decoded[output_index++] = RMT_SYMBOL_ZERO;
            }
            
            //if duration1 = 20, then we are starting low
            if (symbols[i].duration1 != RMT_DURATION_SYMBOL){
                curr_high_low = !curr_high_low;
            }
        #else
            //nrz-i encoding - bit stuffing doesn't work
            //there is always a rising edge (period of RMT_DURATION_SYMBOL on high as the first half isn't captured)
            // if (i == 0){
            //     curr_high_low = true;
            //     if (symbols[i].duration0 == RMT_DURATION_MAX){
            //         //next symbol is a 1 - can continue (first RMT_DURATION is from the first symbol (init rising edge). second RMT_DURATION is second symbol)
            //         i++;
            //         continue;
            //     }
            // }
            
            //need to "split"
            if (symbols[i].duration0 % (RMT_DURATION_SYMBOL * 2) != 0){
                num_0_symbols_duration = symbols[i].duration0 - RMT_DURATION_SYMBOL; //last waveform has duration0 with some duration that's only a multiple of RMT_DURATION_SYMBOL
            }else {
                num_0_symbols_duration = symbols[i].duration0 - RMT_DURATION_SYMBOL * 2; //one from the rising edge, one from the falling edge
            }

            
            num_0_symbols = num_0_symbols_duration / RMT_DURATION_MAX; //should be the number of 0 symbols
            for (int j = 0; j < num_0_symbols && output_index < output_num; j++){
                decoded[output_index++] = curr_high_low ? RMT_SYMBOL_ZERO_HIGH : RMT_SYMBOL_ZERO_LOW;
                consecutive_zeros++;
            }

            curr_high_low = !curr_high_low;
            if (output_index >= output_num){
                break;
            }

            if (!curr_high_low){
                decoded[output_index++] = RMT_SYMBOL_ONE_FALLING;
            } else {
                decoded[output_index++] = RMT_SYMBOL_ONE_RISING;
            }

            // if (consecutive_zeros == MAX_ZER){
            //     consecutive_zeros = 0;
            // } else {
            //     if (!curr_high_low) {
            //         decoded[output_index++] = RMT_SYMBOL_ONE_FALLING;
            //     } else {
            //         decoded[output_index++] = RMT_SYMBOL_ONE_RISING;
            //     }
            //     consecutive_zeros = 0;  // reset zero count after a real 1 bit
            // }
            
            if (symbols[i].duration1 == 0){
                break; //last waveform has duration1 = 0
            }
            
            num_0_symbols_duration = symbols[i].duration1 - RMT_DURATION_SYMBOL * 2; //one from the falling edge, one from the rising edge
            num_0_symbols = num_0_symbols_duration / RMT_DURATION_MAX; //should be the number of 0 symbols

            for (int j = 0; j < num_0_symbols && output_index < output_num; j++){
                decoded[output_index++] = curr_high_low ? RMT_SYMBOL_ZERO_HIGH : RMT_SYMBOL_ZERO_LOW;
            }

            curr_high_low = !curr_high_low;
            if (output_index >= output_num){
                break;
            }
            if (!curr_high_low){
                decoded[output_index++] = RMT_SYMBOL_ONE_FALLING;
            } else {
                decoded[output_index++] = RMT_SYMBOL_ONE_RISING;
            }

            // if (consecutive_zeros == 5){
            //     consecutive_zeros = 0;
            // } else {
            //     if (!curr_high_low) {
            //         decoded[output_index++] = RMT_SYMBOL_ONE_FALLING;
            //     } else {
            //         decoded[output_index++] = RMT_SYMBOL_ONE_RISING;
            //     }
            //     consecutive_zeros = 0;  // reset zero count after a real 1 bit
            // }

        #endif //NRZ_INVERTED
        
        i++;
        
    }

    return (int)output_index;
}

/**
 * @brief This converts the parsed symbols into a string of size `output_index`
 * 
 * @param symbols Parsed received symbols (see `RMTSymbols.h` for the definitions of the symbols)
 * @param num Length of `symbols`
 * @param string Output string encoded by the symbols
 * @param output_num `length of the char array`
 * @return int - length of the output string (-1 if failure)
 */
int RMTManager::convert_symbols_to_char(rmt_symbol_word_t* symbols, size_t num, uint8_t* string, size_t output_num){
    if (symbols == NULL || string == NULL || num == 0 || output_num == 0){
        return ESP_FAIL;
    }
    size_t bit_count = 0;
    char byte = 0;
    size_t output_index = 0;
    int i = 0;
    
    while (i < num && output_index < output_num){
        #ifndef NRZ_INVERTED
            if (symbols[i].level0 == 0 && symbols[i].level1 == 1){
                //zero
                byte = byte << 1;
            }else if (symbols[i].level0 == 1 && symbols[i].level1 == 0) {
                byte = (byte << 1) + 1;
            } else {
                return ESP_FAIL;
            }
        #else
            //nrz-i
            
            if (symbols[i].level0 != symbols[i].level1){
                //bit 1
                byte = (byte << 1) + 1;
            } else if (symbols[i].level0 == symbols[i].level1){
                //bit 0
                byte = byte << 1;
            } else {
                return ESP_FAIL;
            }
        #endif //NRZ_INVERTED

        bit_count++;
        if (bit_count == 8){
            //a byte has been parsed
            // printf("inserting %b\n", byte);
            string[output_index++] = byte;
            byte = 0;
            bit_count = 0;
        }
        i++;
    }
    printf("output_index %d\n", output_index);
    return (int)output_index;
}

/**
 * @brief Start async RX job
 * 
 * @return esp_err_t 
 */
esp_err_t RMTManager::start_receiving(uint8_t channel_num){
    if (channel_num >= MAX_CHANNELS){
        return ESP_FAIL;
    }

    if (channels[channel_num].status == CHANNEL_LISTENING){
        return ESP_OK; //failed to receive earlier; no need to start the async rx job again (alreayd running)
    }

    if (channels[channel_num].status == CHANNEL_NOT_READY_STATUS){
        ESP_LOGE(DEBUG_TAG, "RX Channel is not ready");
        return ESP_FAIL;
    }

    if (channels[channel_num].rx_rmt_handle == NULL){
        ESP_LOGE(DEBUG_TAG, "RX Channel not ready");
        return ESP_FAIL;
    }

    esp_err_t res = rmt_receive(channels[channel_num].rx_rmt_handle, channels[channel_num].raw_symbols, sizeof(channels[channel_num].raw_symbols), &this->receive_config);

    if (res != ESP_OK){
        // printf("Failed to start receive\n");
        ESP_LOGE(DEBUG_TAG, "Failed to start receive");
    }

    channels[channel_num].status = CHANNEL_LISTENING;

    return res;
}

/**
 * @brief Function to get the received messages
 * 
 * @return int 
 */
int RMTManager::receive(uint8_t* recv_buf, size_t size, size_t* output_size, uint8_t channel_num){
    if (channel_num >= MAX_CHANNELS){
        return ESP_FAIL;
    }

    if (channels[channel_num].status != CHANNEL_LISTENING){
        ESP_LOGE(DEBUG_TAG, "receive(): Receive channel %d is not ready to receive due to init fail or async job was not started", channel_num);
        return ESP_FAIL;
    }

    rmt_rx_done_event_data_t rx_data;
    if (xQueueReceive(channels[channel_num].rx_queue, &rx_data, pdMS_TO_TICKS(15000)) != pdTRUE){ //this will wait until a message has arrived or not
        // printf("Timeout occurred while waiting for RX event\n");
        ESP_LOGE(DEBUG_TAG, "Timeout occurred while waiting for RX event - didn't receive a message in time");
        return ESP_FAIL;
    }

    channels[channel_num].status = CHANNEL_READY_STATUS;

    // printf("Got %d symbols\n", rx_data.num_symbols);
    // printf("raw symbols:\n");
    // for (int i = 0; i < rx_data.num_symbols; i++){
    //     printf("duration0 %d level0 %d duration1 %d level1 %d\n", rx_data.received_symbols[i].duration0, rx_data.received_symbols[i].level0, rx_data.received_symbols[i].duration1, rx_data.received_symbols[i].level1);
    // }

    int num = this->decode_symbols(rx_data.received_symbols, rx_data.num_symbols, channels[channel_num].decoded_recv_symbols, sizeof(channels[channel_num].decoded_recv_symbols));
    if (num < 0){
        return ESP_FAIL;
    }
    
    // printf("\n\nparsed symbols:\n");
    // for (int i = 0; i < num; i++){
    //     printf("duration0 %d level0 %d duration1 %d level1 %d\n", decoded_recv_symbols[i].duration0, decoded_recv_symbols[i].level0, decoded_recv_symbols[i].duration1, decoded_recv_symbols[i].level1);
    // }

    *output_size = this->convert_symbols_to_char(channels[channel_num].decoded_recv_symbols, num, recv_buf, size);
    if (*output_size < 0){
        return ESP_FAIL;
    }

    return ESP_OK;
}

RMTManager::~RMTManager(){
    for (uint8_t i = 0; i < MAX_CHANNELS; i++){
        if (this->channels[i].tx_rmt_handle) {
            rmt_disable(this->channels[i].tx_rmt_handle);
            rmt_del_channel(this->channels[i].tx_rmt_handle);
        }
        if (channels[i].rx_rmt_handle) {
            rmt_disable(channels[i].rx_rmt_handle);
            rmt_del_channel(channels[i].rx_rmt_handle);
        }
        if (channels[i].rx_queue) {
            vQueueDelete(channels[i].rx_queue);
        }
    }
}