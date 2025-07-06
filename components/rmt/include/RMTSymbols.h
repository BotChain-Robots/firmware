#ifdef RMT_COMMUNICATIONS

#include "driver/rmt_tx.h"

// #ifdef MANCHESTER_40
// #define RMT_RESOLUTION_HZ 40 * 1000 * 1000 // 40MHz resolution
// #define RMT_DURATION_SYMBOL 10 //0.25 us bit duration
// #elif NRZ_INVERTED
// #ifdef NRZ_INVERTED_40_HZ
// #define RMT_RESOLUTION_HZ 40 * 1000 * 1000 // 40MHz resolution
// #else
// #define RMT_RESOLUTION_HZ 20 * 1000 * 1000 // 20MHz resolution
// #endif //NRZ_INVERTED_40_HZ
// #ifdef NRZ_INVERTED_10
// #define RMT_DURATION_SYMBOL 10 //0.5us bit duration
// #elif NRZ_INVERTED_20
// #define RMT_DURATION_SYMBOL 5 //0.25 us bit duration
// #elif NRZ_INVERTED_2
// #define RMT_DURATION_SYMBOL 2 //0.1 us bit duration
// #else
// #define RMT_DURATION_SYMBOL 20 //1us bit duration
// #endif //NRZ_INVERTED_10
// #else
// #define RMT_RESOLUTION_HZ 1 * 1000 * 1000 // 1MHz resolution
// #define RMT_DURATION_SYMBOL 10
// #endif //MANCHESTER_40

// #define NRZ_INVERTED //using NRZ_I
#define RMT_RESOLUTION_HZ 40 * 1000 * 1000 // 40MHz resolution
#define RMT_DURATION_SYMBOL 12 //0.6us


// #define RMT_DURATION_SYMBOL ((RMT_RESOLUTION_HZ * 3) / 1000000) // duration time for a symbol - this is 3us

#define RMT_DURATION_MAX (2 * RMT_DURATION_SYMBOL)

// #define RMT_TX_GPIO GPIO_NUM_1 //RMT will use GPIO pin 1 to transmit (on one channel)

// #define RMT_RX_GPIO GPIO_NUM_12 // RMT will use GPIO pin 12 to receive (on one channel)

#ifndef NRZ_INVERTED
//MANCHESTER ENCODING (ETHERNET STANDARD)

/**
 * @brief This struct represents a 1 symbol being transmitted over RMT. This will create a falling edge (low for `RMT_DURATION_SYMBOL` and high for `RMT_DURATION_SYMBOL`)
 * 
 */
static const rmt_symbol_word_t RMT_SYMBOL_ONE = {
    .duration0 = RMT_DURATION_SYMBOL,
    .level0 = 1,
    .duration1 = RMT_DURATION_SYMBOL,
    .level1 = 0,
};

/**
 * @brief This struct represents a 0 symbol being transmitted over RMT. This will create a rising edge (low for `RMT_DURATION_SYMBOL` and high for `RMT_DURATION_SYMBOL`)
 * 
 */
static const rmt_symbol_word_t RMT_SYMBOL_ZERO = {
    .duration0 = RMT_DURATION_SYMBOL,
    .level0 = 0,
    .duration1 = RMT_DURATION_SYMBOL,
    .level1 = 1,
};
#else

//Non-Return-to-Zero Inverted (NRZ-I)

#define CONSEC_ZERO_THRESHOLD 3 //max number of consecutive zeros before adding a bit 1

// Logic 1 inverts the current voltage state

/**
 * @brief This struct represents a 1 symbol being transmitted over RMT. This will create a falling edge (low for `RMT_DURATION_SYMBOL` and high for `RMT_DURATION_SYMBOL`)
 * 
 */
static const rmt_symbol_word_t RMT_SYMBOL_ONE_FALLING = {
    .duration0 = RMT_DURATION_SYMBOL,
    .level0 = 1,
    .duration1 = RMT_DURATION_SYMBOL,
    .level1 = 0,
};

/**
 * @brief This struct represents a 1 symbol being transmitted over RMT. This will create a rising edge (low for `RMT_DURATION_SYMBOL` and high for `RMT_DURATION_SYMBOL`)
 * 
 */
static const rmt_symbol_word_t RMT_SYMBOL_ONE_RISING = {
    .duration0 = RMT_DURATION_SYMBOL,
    .level0 = 0,
    .duration1 = RMT_DURATION_SYMBOL,
    .level1 = 1,
};

/**
 * @brief This struct will represent a bit 0. In NRZ-I, this represents a no change in the voltage
 * 
 */
static const rmt_symbol_word_t RMT_SYMBOL_ZERO_HIGH = {
    .duration0 = RMT_DURATION_SYMBOL,
    .level0 = 1,
    .duration1 = RMT_DURATION_SYMBOL,
    .level1 = 1,
};

/**
 * @brief This struct will represent a bit 0. In NRZ-I, this represents a no change in the voltage
 * 
 */
static const rmt_symbol_word_t RMT_SYMBOL_ZERO_LOW = {
    .duration0 = RMT_DURATION_SYMBOL,
    .level0 = 0,
    .duration1 = RMT_DURATION_SYMBOL,
    .level1 = 0,
};


#endif //NRZ_INVERTED

//not used at the moment

// static const rmt_symbol_word_t RMT_SYMBOL_HIGH_STOP = {
//     .duration0 = RMT_DURATION_SYMBOL / 2,
//     .level0 = 1,
//     .duration1 = RMT_DURATION_SYMBOL / 2,
//     .level1 = 0,
// };

// static const rmt_symbol_word_t RMT_SYMBOL_LOW_STOP = {
//     .duration0 = RMT_DURATION_SYMBOL / 2,
//     .level0 = 0,
//     .duration1 = RMT_DURATION_SYMBOL / 2,
//     .level1 = 1,
// };

#endif //RMT_COMMUNICATIONS