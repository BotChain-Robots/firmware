#ifdef RMT_COMMUNICATIONS

#include "driver/rmt_tx.h"

#define RMT_RESOLUTION_HZ 4 * 1000 * 1000 // 4 MHz resolution
#define RMT_DURATION_SYMBOL 2 //1 us

#define RMT_DURATION_MAX (2 * RMT_DURATION_SYMBOL)

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