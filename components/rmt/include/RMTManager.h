#ifndef RMT_COMMUNICATIONS
#define RMT_COMMUNICATIONS

#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"

#include "soc/gpio_num.h"
#include "RMTSymbols.h"

#include <cstring>

#define MAX_CHANNELS 4
#define RMT_SYMBOL_BLOCK_SIZE 48

#define RECEIVE_BUFFER_SIZE 1024 //this is some value (we should probably set it to some packet size that we predetermine in some custom protocol:tm:)
#define DEBUG_TAG "RMTManager"

#define CHANNEL_LISTENING (0x2) //channel waiting to receive
#define CHANNEL_READY_STATUS (0x1) //channel able to send and ready to start receive async job
#define CHANNEL_NOT_READY_STATUS (0x0) //channel is not ready (cannot send and/or receive)

#define QUEUE_SIZE 10

/**
 * @brief This struct keeps track of the current byte and bit index of the user data being transmmitted via RMT
 * 
 */
typedef struct {
    size_t byte_index; //which byte is currently being encoded when transmitting
    uint8_t bit_index; //which bit in the `byte_index` is currently being encoded (into high/low waveforms)
    size_t num_symbols; //temp
    #ifdef NRZ_INVERTED
    bool current_level;
    uint8_t zero_count;
    #endif //NRZ_INVERTED
} rmt_encoder_context_t;

typedef struct _rmt_channel{
    //TX
    uint8_t tx_gpio;
    rmt_channel_handle_t tx_rmt_handle;
    SemaphoreHandle_t tx_done_semaphore;
    QueueHandle_t tx_queue;
    rmt_encoder_handle_t encoder; //encoder config
    rmt_encoder_context_t encoder_context;

    //RX
    uint8_t rx_gpio;
    rmt_channel_handle_t rx_rmt_handle;
    QueueHandle_t rx_queue;
    rmt_symbol_word_t raw_symbols[RECEIVE_BUFFER_SIZE]; //buffer to store the symbols on receive
    rmt_symbol_word_t decoded_recv_symbols[RECEIVE_BUFFER_SIZE]; //allocating some dummy size buffer for decoded string
    
    //General
    uint8_t status;
} rmt_channel;


class RMTManager{
    public:
        RMTManager(uint8_t num_channels);
        ~RMTManager();
        esp_err_t send(uint8_t* data, size_t size, rmt_transmit_config_t* config, uint8_t channel_num); //temp function to send some string data
        esp_err_t receive(uint8_t* recv_buf, size_t size, size_t* output_size, uint8_t channel_num);

        static size_t encoder_callback(const void* data, size_t data_size, size_t symbols_written, 
            size_t symbols_free, rmt_symbol_word_t* symbols, bool* done, void* arg);

        static bool rmt_rx_done_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data);
        static bool rmt_tx_done_callback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_data);

        esp_err_t start_receiving(uint8_t channel_num);

        esp_err_t wait_until_send_complete(uint8_t channel_num);
        
    private:
        uint8_t num_channels; //number of channels initalized
        esp_err_t init();
        void reset_encoder_context(rmt_encoder_context_t* ctx);
        esp_err_t init_tx_channel();
        esp_err_t init_rx_channel();
        int decode_symbols(rmt_symbol_word_t* symbols, size_t num, rmt_symbol_word_t* decoded, size_t output_num);
        int convert_symbols_to_char(rmt_symbol_word_t* symbols, size_t num, uint8_t* string, size_t output_num);
        
        rmt_channel channels[MAX_CHANNELS] = {0};
        //=====================TX=====================

        // rmt_channel_handle_t tx_chan;

        const gpio_num_t tx_gpio[MAX_CHANNELS] = {GPIO_NUM_3, GPIO_NUM_5, GPIO_NUM_11, GPIO_NUM_13}; //using pins 1,2,3,4 for channels 0,1,2,3 respectively for tx
        // gpio_num_t tx_gpio[MAX_CHANNELS] = {GPIO_NUM_1}; //using pins 1,2,3,4 for channels 0,1,2,3 respectively for tx

        // rmt_encoder_context_t encoder_context = {0};

        //semaphore to indicate it is done
        // SemaphoreHandle_t tx_done_semaphore;
        
        //will be used to temporarily hold the bits that are being wait to be sent -- not working
        // QueueHandle_t transmit_queue = NULL;

        // TxCallbackContext tx_context;

        //=====================RX=====================
        rmt_channel_handle_t rx_chan;

        const gpio_num_t rx_gpio[MAX_CHANNELS] = {GPIO_NUM_4, GPIO_NUM_6, GPIO_NUM_12, GPIO_NUM_14}; //using pins 12,13,14,15 for channels 0,1,2,3 respectively for rx
        // gpio_num_t rx_gpio[MAX_CHANNELS] = {GPIO_NUM_12}; //using pins 12,13,14,15 for channels 0,1,2,3 respectively for rx

        // QueueHandle_t receive_queue = NULL;

        //rx_receive_config
        rmt_receive_config_t receive_config = {
            .signal_range_min_ns = 100,
            .signal_range_max_ns = 200 * 1000, 
            .flags = {
                .en_partial_rx = true
            }
        };

        // bool ready_to_receive = false;
};

//will need to keep the data alive until it has been transmitted (not working or being used atm)

struct TxCallbackContext{
    SemaphoreHandle_t tx_done_sem;
    QueueHandle_t transmit_queue;
    rmt_encoder_context_t* tx_context;
};

typedef struct {
    const uint8_t* data;
    size_t length;
} TxBuffer;

typedef struct _gpio_channel_pair {
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
} GPIO_Channel_Pair;

static const GPIO_Channel_Pair gpio_channel_pairs[MAX_CHANNELS] = {
    {
        .tx_pin = GPIO_NUM_1,
        .rx_pin = GPIO_NUM_12
    },
    {
        .tx_pin = GPIO_NUM_2,
        .rx_pin = GPIO_NUM_13
    },
    {
        .tx_pin = GPIO_NUM_3,
        .rx_pin = GPIO_NUM_14
    },
    {
        .tx_pin = GPIO_NUM_4,
        .rx_pin = GPIO_NUM_15
    }
}; //todo: use these pairs directly instead of the two arrays in the class definition above

#endif //RMT_COMMUNICATIONS
