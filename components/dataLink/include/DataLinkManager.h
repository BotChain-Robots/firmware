#ifndef DATA_LINK
#define DATA_LINK

#include <queue>
#include <memory>
#include <unordered_map>

#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "Frames.h"
#include "Tables.h"
#include "RMTManager.h"
#include "BlockingQueue.h"
#include "BlockingPriorityQueue.h"
#include <unordered_map>
#include "Scheduler.h"

#define DEBUG_LINK_TAG "LinkLayer"

#define CRC_POLYNOMIAL 0x1021

static const char* NVS_BOARD_ID_KEY = "id";
static const char* NVS_BOARD_NAMESPACE = "board";

//look up table for crc
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

#define ASYNC_QUEUE_WAIT_TICKS 100
#define SEQUENCE_NUM_MAP_MUTEX_MAX_WAIT_MS 50
#define MAX_RX_QUEUE_SIZE 100

/**
 * @brief Class to represent the Data Link Layer
 *
 * @author Justin Chow
 */
class DataLinkManager{
    public:
        DataLinkManager(uint8_t board_id, uint8_t num_channels);
        ~DataLinkManager();
        esp_err_t send(uint8_t dest_board, std::unique_ptr<std::vector<uint8_t>>&& buffer, FrameType type, uint8_t flag);
        esp_err_t start_receive_frames(uint8_t curr_channel);
        esp_err_t receive(uint8_t* data, size_t data_len, size_t* recv_len, uint8_t curr_channel);
        esp_err_t print_frame_info(uint8_t* data, size_t data_len, uint8_t* message, size_t message_len);
        esp_err_t get_routing_table(RIPRow_public* table, size_t* table_size);
        std::optional<std::unique_ptr<std::vector<uint8_t>>> async_receive();
        esp_err_t ready();
        esp_err_t send_ack(uint8_t sender_id, uint8_t* data, uint16_t data_len);
    private:
        uint8_t this_board_id = 0;
        uint8_t num_channels = MAX_CHANNELS;
        std::unique_ptr<RMTManager> phys_comms;

        std::unordered_map<uint8_t, uint16_t> sequence_num_map;
        SemaphoreHandle_t sequence_num_map_mutex;
        esp_err_t get_inc_sequence_num(uint8_t board_id, uint16_t* seq_num);
        esp_err_t get_sequence_num(uint8_t board_id, uint16_t* seq_num);

        volatile bool stop_tasks = false; //used by the tasks to know when to stop (set true when DataLinkManager is destroyed)
        TaskHandle_t rip_broadcast_task = NULL;
        TaskHandle_t rip_ttl_task = NULL;

        esp_err_t set_board_id(uint8_t board_id);
        esp_err_t get_board_id(uint8_t& board_id);
        void print_binary(uint8_t byte);
        void print_buffer_binary(const uint8_t* buffer, size_t length);
        esp_err_t get_data_from_frame(uint8_t* data, size_t data_len, uint8_t* message, size_t* message_size, FrameHeader* header);
        esp_err_t geneate_crc_16(uint8_t* data, size_t data_len, uint16_t* crc);
        esp_err_t create_control_frame(uint8_t* data, uint16_t data_len, ControlFrame control_frame, uint8_t* send_data, size_t* send_data_len);
        esp_err_t create_generic_frame(uint8_t* data, uint16_t data_len, GenericFrame generic_frame, uint16_t offset, uint8_t* send_data, size_t* send_data_len);

        //==== RIP related functions ====

        void init_rip();
        esp_err_t rip_find_entry(uint8_t board_id, RIPRow** entry, bool reserve_row);
        esp_err_t rip_update_entry(uint8_t new_hop, uint8_t channel, RIPRow** entry);
        esp_err_t rip_add_entry(uint8_t board_id, uint8_t hops, uint8_t channel, RIPRow** entry);
        esp_err_t rip_reset_entry_ttl(uint8_t board_id);
        esp_err_t rip_get_row(RIPRow** entry, uint8_t row_num);

        //this is stored locally with metadata `ttl`
        // std::unordered_map<uint8_t, RIPRow> rip_table; //using a hash map to store the routes to other boards - will be used as we scale up
        RIPRow rip_table[RIP_MAX_ROUTES]; //temp using a static array

        void start_rip_tasks();
        esp_err_t send_rip_frame(bool broadcast, uint8_t dest_id);
        [[noreturn]] static void rip_broadcast_timer_function(void* args);
        [[noreturn]] static void rip_ttl_decrement_task(void* args);
        QueueHandle_t manual_broadcasts;

        QueueHandle_t discovery_tables;

        esp_err_t route_frame(uint8_t dest_id, uint8_t* channel_to_send);

        //==== Frame Scheduling related functions ====

        /**
         * @brief Priority queue for each channel to schedule when to send frames
         *
         */
        std::unique_ptr<BlockingPriorityQueue<SchedulerMetadata, std::vector<SchedulerMetadata>, FrameCompare>> frame_queue[MAX_CHANNELS];
        void init_scheduler();
        esp_err_t push_frame_to_scheduler(SchedulerMetadata frame, uint8_t channel);
        TaskHandle_t scheduler_task = NULL;

        [[noreturn]] static void frame_scheduler(void* args);

        esp_err_t scheduler_send(uint8_t channel);

        esp_err_t scheduler_send_rmt(uint8_t channel, SchedulerMetadata frame, uint8_t* send_data, size_t frame_size, bool wait_for_tx_done);

        //Generic Frame Receive Fragments

        esp_err_t store_fragment(GenericFrame* fragment, uint8_t channel);

        /**
         * @brief Stores generic frame fragments
         *
         * Mapping:
         * Board ID (of the receiver) -> Sequence number -> Array of Generic Frame Fragments, with size of the number of expected fragments
         *
         * TODO:
         * - Sliding window + ACKs
         *
         */
        std::unordered_map<uint16_t, std::unordered_map<uint16_t, FragmentMetadata>> fragment_map[MAX_CHANNELS];

        esp_err_t complete_fragment(uint16_t board_id, uint16_t sequence_num, uint8_t channel);

        SemaphoreHandle_t async_rx_queue_mutex[MAX_CHANNELS];
        SemaphoreHandle_t rx_fragment_mutex[MAX_CHANNELS];

        //Async receive
        /**
         * @brief Queue to store complete received frame data
         *
         */
        std::unique_ptr<BlockingQueue<Rx_Metadata>> async_receive_queue;

        esp_err_t start_receive_frames_rmt(uint8_t curr_channel);

        /**
         * @brief Receive thread entry point
         *
         * @param args
         */
        [[noreturn]] static void receive_thread_main(void* args);

        /**
         * @brief Receive bytes from Physical Layer (RMT)
         *
         * @note This replaces the deprecated `receive` function
         *
         * @param channel Physical channel pair to look at
         * @return esp_err_t
         */
        esp_err_t receive_rmt(uint8_t channel);

        TaskHandle_t receive_task = NULL;

        /**
         * @brief Generic Frame Sliding Window
         *
         * Mapping:
         * Board Id (of the receiver) -> Sequence Number -> FrameAckRecord
         *
         */
        std::unordered_map<uint16_t, std::unordered_map<uint16_t, FrameAckRecord>> sliding_window[MAX_CHANNELS];

        SemaphoreHandle_t sliding_window_mutex[MAX_CHANNELS];

        esp_err_t inc_head_sliding_window(uint8_t channel, uint8_t board_id, uint16_t seq_num, FrameAckRecord* ack_record);

        esp_err_t get_record_sliding_window(uint8_t channel, uint8_t board_id, uint16_t seq_num, FrameAckRecord* ack_record);

        esp_err_t complete_record_sliding_window(uint8_t channel, uint8_t board_id, uint16_t seq_num);

        /**
         * @brief Thread for sending acks - Send ACKs on a separate thread to not hold up the receive thread (missing other frames)
         *
         * @param args
         */
        [[noreturn]] static void send_ack_thread_main(void* args);
        TaskHandle_t send_ack_task = NULL;

        SemaphoreHandle_t send_ack_queue_mutex[MAX_CHANNELS];
        std::queue<SendAckMetaData> send_ack_queue[MAX_CHANNELS];
};

struct frame_scheduler_args {
    uint8_t channel_id;
    DataLinkManager* that;
};

#endif //DATA_LINK
