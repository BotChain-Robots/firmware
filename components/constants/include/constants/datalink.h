#ifndef DATALINK_H
#define DATALINK_H
#include "module.h"

/**
 * @brief Frame Constants
 */
 
#define BROADCAST_ADDR 0xFF //used for discovery (finding the board's neighbours). this will mean the board ids will have 2^8-2 = 254 unique IDs that could be assigned

#define START_OF_FRAME 0xAB //0b1010_1011 - denotes the start of frame

#define MAX_FRAME_SIZE 121 //Max 121B (due to rmt) - note this includes the overhead of the frame. the actual payload max depends on the frame type (eg. 121 - 9 B is the max control data length)

#define MAX_GENERIC_NUM_FRAG (1 << 16) // Max 2**16 Fragments can be made with a generic frame (total 2**16 *MAX_GENERIC_DATA_LEN B of data can be sent ~ 6.7 MiB)

#define MAX_FRAME_QUEUE_SIZE 15 //Size of the queue for the frame scheduler (per channel)

//Flags
#define FLAG_FRAG 0x8 //0b1000 //this fragmented frame is part of a larger frame
#define FLAG_DISCOVERY 0x4 //0b0100
#define FLAG_NEIGH_TABLE 0x2 //0b0010 - used to denote the frame contains the neighbour tables (used for finding the configuration/topology of the network); similar to an ARP or MAC table
#define FLAG_ACK 0x1 //0b0001_0000 - used for confirming receipt of different types of frames from the neighbours

#define GET_TYPE(x) ((x) & 0xF0)
#define GET_FLAG(x) ((x) & 0x0F)
#define MAKE_TYPE_FLAG(type, flag) ((uint8_t)((type & 0xF0) | (flag & 0xF)))
#define IS_CONTROL_FRAME(x) (((x) & 0x80) != 0)

#define CONTROL_FRAME_OVERHEAD 9
#define GENERIC_FRAME_OVERHEAD 14

#define MAX_GENERIC_DATA_LEN (MAX_FRAME_SIZE - GENERIC_FRAME_OVERHEAD)
#define MAX_CONTROL_DATA_LEN (MAX_FRAME_SIZE - CONTROL_FRAME_OVERHEAD)

//Generic Frame Fragment ACK
#define GENERIC_FRAG_ACK_DATA_SIZE 7
#define GENERIC_FRAG_ACK_PREAMBLE 0x69

#define CONTROL_FRAME_TYPE 0x80 //if the frame type MSB is set to 1, use the control frame

/**
 * @brief Scheduler Constants
 */
 
#define SCHEDULER_MUTEX_WAIT 10 //max time duration to wait
#define SCHEDULER_PERIOD_MS 140
#define RECEIVE_TASK_PERIOD_MS 2

#define GENERIC_FRAME_SLIDING_WINDOW_SIZE 5 //defines the maximum size of the sliding window before resending previously un-ack'd fragments
#define SLIDING_WINDOW_MUTEX_TIMEOUT_MS 5
#define GENERIC_FRAME_MOD_TIMEOUT 10 //be scheduled at most 9 + GENERIC_FRAME_MIN_TIMEOUT times before sending another fragment
#define GENERIC_FRAME_MIN_TIMEOUT 10

#define SEND_ACK_PERIOD_MS 50
#define SEND_ACK_MUTEX_WAIT 10

/**
 * @brief RIP Table Constants
 */

#define RIP_MAX_HOPS 15 //16 or more is infinite
#define RIP_MAX_ROUTES 10 //for the demo we will use up to 10 boards in total (9 other boards will be connected = 9 rows)
#define RIP_INVALID_ROW 0
#define RIP_VALID_ROW 1
#define RIP_NEW_ROW 2
#define RIP_BROADCAST_INTERVAL 30000 //broadcast every 30 seconds (30000ms)
// #define RIP_BROADCAST_INTERVAL 3000 //temp broadcast every 3 seconds (3000ms)
#define RIP_TTL_START 180 //seconds
#define RIP_MS_TO_SEC 1000 //1000 ms to 1 sec
#define RIP_MAX_SEM_WAIT_MS 30
#define RIP_FLUSH_COUNT 8 //flush after 8*30 seconds = 240 seconds

#define RIP_DISCOVERY_MESSAGE_SIZE 1

#endif //DATALINK_H