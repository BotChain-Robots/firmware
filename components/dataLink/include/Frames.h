#ifdef DATA_LINK

#include "freertos/FreeRTOS.h"
#include <variant>
#include <cstdint>

#define BROADCAST_ADDR 0xFF //used for discovery (finding the board's neighbours). this will mean the board ids will have 2^8-2 = 254 unique IDs that could be assigned
#define PC_ADDR 0x0 //setting 0 to be the PC

#define START_OF_FRAME 0xAB //0b1010_1011 - denotes the start of frame

#define MAX_GENERIC_DATA_LEN (180) //Max 180B
#define MAX_CONTROL_DATA_LEN (1 << 5) // Max 32B

//Flags
#define FLAG_FRAG 0x8 //0b1000 //this fragmented frame is part of a larger frame
#define FLAG_DISCOVERY 0x4 //0b0100
#define FLAG_NEIGH_TABLE 0x2 //0b0010 - used to denote the frame contains the neighbour tables (used for finding the configuration/topology of the network); similar to an ARP or MAC table
#define FLAG_ACK 0x1 //0b0001_0000 - used for confirming receipt of different types of frames from the neighbours

#define GET_TYPE(x) ((x) & 0xF0)
#define GET_FLAG(x) ((x) & 0x0F)
#define MAKE_TYPE_FLAG(type, flag) ((type) | (flag))
#define IS_CONTROL_FRAME(x) (((x) & 0x80) != 0)

#define CONTROL_FRAME_OVERHEAD 9

#define CONTROL_FRAME_TYPE 0x80 //if the frame type MSB is set to 1, use the control frame
//Types (total 2^4 = 16 different types)
enum class FrameType : uint8_t {
    MOTOR_TYPE = 0x80, //0b1000_0000
    SERVO_TYPE = 0xC0, //0b1100_0000
    DISTANCE_SENSOR_TYPE = 0xE0, //0b1110_0000
    DEBUG_CONTROL_TYPE = 0xC0, //0b1100_0000
    DEBUG_GENERIC_TYPE = 0x00, //0b0000_0000
    SYSTEM_TYPE = 0x30, //0x0011_0000 - used for statuses, discovery, and other maintainence requests
    RIP_TABLE_CONTROL = 0x90, //0b1001_0000 - using the control frame to broadcast the RIP table
    RIP_TABLE_GENERIC = 0x10 //0b0001_000 - using the generic frame to broadcast the RIP table
};

#pragma pack(push, 1) //these structs will be transmitted as is (ensure the structs are structured using 1B alignment - no padding)
typedef struct _control_frame{
    uint8_t preamble; //Start of Frame
    uint8_t sender_id; //sender board id
    uint8_t receiver_id; //receiver board id
    uint16_t seq_num; //sequence number to differentiate frames being sent from sender to receiver
    uint8_t type_flag; //(type << 4) | flag - both are 4 bits
    uint8_t data_len; //Data Length (max 32B)
    uint8_t data[MAX_CONTROL_DATA_LEN]; //Variable Length of Data
    uint16_t crc_16; //CRC-16
} control_frame; //this will have a max size of 9 + 32B = 41B

typedef struct _data_link_frame{
    uint8_t preamble; //Start of Frame
    uint8_t sender_id; //sender board id
    uint8_t receiver_id; //receiver board id
    uint16_t seq_num; //sequence number to differentiate frames being sent from sender to receiver
    uint8_t type_flag; //(type << 4) | flag - both are 4 bits
    uint16_t frag_info; //(total_frag_num << 8) | frag_num - total_frag_num denotes the total number of fragmented frames to expect for this sequence number(?) and frag_num denotes the fragment frame  num
    uint16_t data_len; //Data Length (max 178B)
    uint8_t data[MAX_GENERIC_DATA_LEN]; //Variable Length of Data
    uint16_t crc_16; //CRC-16
} data_link_frame; //this will have a max size of 12 + 180 B = 192B
#pragma pack(pop)

using Frame = std::variant<control_frame, data_link_frame>;

//defining a comparison operation for comparing two frames -- not tested
struct FrameCompare {
    bool operator()(const Frame& a, const Frame& b) const {
        auto msb_set = [](uint8_t type_flag) {
            return (type_flag & 0x80) != 0; // 0x80 == 1000 0000
        };

        auto get_type_flag = [](const Frame& pkt) -> uint8_t {
            return std::visit([](auto&& p) -> uint8_t {
                return p.type_flag;
            }, pkt);
        };

        uint8_t type_flag_a = get_type_flag(a);
        uint8_t type_flag_b = get_type_flag(b);

        bool a_msb = msb_set(type_flag_a);
        bool b_msb = msb_set(type_flag_b);

        if (a_msb != b_msb) {
            return !a_msb; // Frame with MSB set (true) should come first
        }

        // Tie-breaker: use seq_num if MSB is the same
        return std::visit([](auto&& p1, auto&& p2) {
            return p1.seq_num > p2.seq_num; // smaller seq_num = higher priority (older)
        }, a, b);
    }
};

#endif //DATA_LINK