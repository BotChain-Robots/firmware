#ifdef DATA_LINK
#pragma once
#include "freertos/FreeRTOS.h"
#include <variant>
#include <cstdint>
#include <vector>
#include "constants/datalink.h"

//Types (total 2^4 = 16 different types)
enum class FrameType : uint8_t {
    //Control Frames
    MOTOR_TYPE = 0x80, //0b1000_0000
    RIP_TABLE_CONTROL = 0x90, //0b1001_0000 - using the control frame to broadcast the RIP table
    DISTANCE_SENSOR_TYPE = 0xA0, //0b1010_0000
    SERVO_TYPE = 0xC0, //0b1100_0000
    MISC_CONTROL_TYPE = 0xD0, //0b1101_0000
    
    //Generic Frames
    MISC_GENERIC_TYPE = 0x00, //0b0000_0000
    MISC_UDP_GENERIC_TYPE = 0x10, // 0b0001_0000 - Same as MISC_GENERIC_TYPE except no ACK frames will be expected
    SYSTEM_TYPE = 0x30, //0b0011_0000 - used for statuses, discovery, and other maintainence requests
    ACK_TYPE = 0x60, //0b0110_0000 - ACK frames for Generic Fragments
    RIP_TABLE_GENERIC = 0x70 //0b0111_0000 - using the generic frame to broadcast the RIP table (not used rn)
};

enum class FrameFlags : uint8_t {
    ANY_FLAG = 0x0,
};

#pragma pack(push, 1) //these structs will be transmitted as is (ensure the structs are structured using 1B alignment - no padding)
typedef struct _control_frame{
    uint8_t preamble; //Start of Frame
    uint8_t sender_id; //sender board id
    uint8_t receiver_id; //receiver board id
    uint16_t seq_num; //sequence number to differentiate frames being sent from sender to receiver
    uint8_t type_flag; //(type << 4) | flag - both are 4 bits
    uint16_t data_len; //Data Length (max 256B)
    uint8_t data[MAX_FRAME_SIZE]; //Variable Length of Data
    uint16_t crc_16; //CRC-16
} ControlFrame; //this will have a max size of 9 + 256B = 265B

typedef struct _data_link_frame{
    uint8_t preamble; //Start of Frame
    uint8_t sender_id; //sender board id
    uint8_t receiver_id; //receiver board id
    uint16_t seq_num; //sequence number to differentiate frames being sent from sender to receiver
    uint8_t type_flag; //(type << 4) | flag - both are 4 bits
    uint16_t total_frag; //total number of fragments for this sequence
    uint16_t frag_num; //current fragment number
    uint16_t data_len; //Data Length (max 178B)
    uint8_t data[MAX_FRAME_SIZE]; //Variable Length of Data
    uint16_t crc_16; //CRC-16
} GenericFrame; //this will have a max size of 14 + 2^8 B = 270 B
#pragma pack(pop)

typedef struct _header{
    uint8_t preamble; //Start of Frame
    uint8_t sender_id; //sender board id
    uint8_t receiver_id; //receiver board id
    uint16_t seq_num; //sequence number to differentiate frames being sent from sender to receiver
    uint8_t type_flag; //(type << 4) | flag - both are 4 bits
    uint32_t frag_info; //(total_frag_num << 16) | frag_num - total_frag_num denotes the total number of fragmented frames to expect for this sequence number(?) and frag_num denotes the fragment frame  num
    uint16_t data_len; //Data Length (max 178B)
    uint16_t crc_16; //CRC-16
} FrameHeader;

using Frame = std::variant<ControlFrame, GenericFrame>;

ControlFrame make_control_frame_from_header(const FrameHeader& header); 

GenericFrame make_generic_frame_from_header(const FrameHeader& header);

typedef struct _fragment_metadata {
    std::vector<GenericFrame> fragments;
    uint16_t num_fragments_rx;
} FragmentMetadata;

typedef struct _receive_metadata{
    uint8_t* data;
    uint16_t data_len;
    FrameHeader header;
} Rx_Metadata;

#endif //DATA_LINK