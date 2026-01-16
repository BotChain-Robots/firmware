#ifndef RMT_H
#define RMT_H

/**
 * @brief RMT Symbol Constants
 */

#define RMT_RESOLUTION_HZ 4 * 1000 * 1000 // 4 MHz resolution
#define RMT_DURATION_SYMBOL 2 //1 us

#define RMT_DURATION_MAX (2 * RMT_DURATION_SYMBOL)

/**
 * @brief RMT Constants
 */

#define MAX_CHANNELS 4
#define RMT_SYMBOL_BLOCK_SIZE 48

#define RECEIVE_BUFFER_SIZE 1024 //this is some value (we should probably set it to some packet size that we predetermine in some custom protocol:tm:)
#define DEBUG_TAG "RMTManager"

#define CHANNEL_LISTENING (0x2) //channel waiting to receive
#define CHANNEL_READY_STATUS (0x1) //channel able to send and ready to start receive async job
#define CHANNEL_NOT_READY_STATUS (0x0) //channel is not ready (cannot send and/or receive)

#define QUEUE_SIZE 10

#define MUTEX_MAX_WAIT_TICKS 100


#endif //RMT_H