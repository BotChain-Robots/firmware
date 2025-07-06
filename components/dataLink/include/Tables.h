#pragma once
#ifdef DATA_LINK
#include "freertos/FreeRTOS.h"

#define RIP_MAX_HOPS 15 //16 or more is infinite
#define RIP_MAX_ROUTES 10 //for the demo we will use up to 10 boards in total (9 other boards will be connected = 9 rows)
#define RIP_INVALID_ROW 0
// #define RIP_BROADCAST_INTERVAL 30000 //broadcast every 30 seconds (30000ms)
#define RIP_BROADCAST_INTERVAL 3000 //temp broadcast every 3 seconds (3000ms)
#define RIP_TTL_START 180 //seconds
#define RIP_MS_TO_SEC 1000 //1000 ms to 1 sec
#define RIP_MAX_SEM_WAIT 30

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
    StaticSemaphore_t mutex_buf; //where mutex state is stored
    SemaphoreHandle_t row_sem; //mutex sem handle of mutex_buf
} RIPRow;

#endif //DATA_LINK
