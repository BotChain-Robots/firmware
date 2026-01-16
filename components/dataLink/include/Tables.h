#pragma once
#ifdef DATA_LINK
#include "freertos/FreeRTOS.h"
#include "constants/datalink.h"

/**
 * @brief Routing data to a board
 * This struct will be sent to other boards
 */
typedef struct _rip_hops{
    uint8_t board_id; //ID of the destination
    uint8_t hops; //hop count to `board_id`
} RIPHop;

typedef struct _rip_row{
    RIPHop info;
    uint8_t channel; //rmt channel
    uint8_t ttl; //how long this entry is valid for. starting value is 180 seconds
    uint8_t valid; //is this a valid entry?
    uint8_t ttl_flush; //if hops is invalid, this would count the amount of time until this entry would be invalid (max is in multiples of 30 seconds) but can vary
    StaticSemaphore_t mutex_buf; //where mutex state is stored
    SemaphoreHandle_t row_sem; //mutex sem handle of mutex_buf
} RIPRow;

/**
 * @brief Public facing RIP table row
 * 
 */
typedef struct _rip_public_row{
    RIPHop info;
    uint8_t channel; //rmt channel
} RIPRow_public;

typedef struct _rip_public_matrix{
    RIPRow_public* table;
    size_t size;
    uint8_t board_id; //Board ID's routing table
} RIPRow_public_matrix;

#endif //DATA_LINK
