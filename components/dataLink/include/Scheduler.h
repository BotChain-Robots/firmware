#ifdef DATA_LINK
#include "Frames.h"
#include "esp_timer.h"
#include <cstdint>

#define SCHEDULER_MUTEX_WAIT 10 //max time duration to wait
#define SCHEDULER_PERIOD_MS 10
#define RECEIVE_TASK_PERIOD_MS 2

#define GENERIC_FRAME_SLIDING_WINDOW_SIZE 5 //defines the maximum size of the sliding window before resending previously un-ack'd fragments
#define SLIDING_WINDOW_MUTEX_TIMEOUT_MS 5
#define GENERIC_FRAME_MOD_TIMEOUT 10 //be scheduled at most 9 + GENERIC_FRAME_MIN_TIMEOUT times before sending another fragment
#define GENERIC_FRAME_MIN_TIMEOUT 10

#define SEND_ACK_PERIOD_MS 50
#define SEND_ACK_MUTEX_WAIT 10

//Metadata representing the frame to be sent but is currently scheduled
typedef struct _frame_scheduler_metadata {
    FrameHeader header; //header of the frame
    uint16_t generic_frame_data_offset; //For data greater than MAX_GENERIC_DATA_LEN to keep track of fragment positions
    int64_t enqueue_time_ns; //when the frame has been first enqueued into the priority queue
    std::shared_ptr<std::vector<uint8_t>> data; // the actual data, and length of data

    //sliding window
    uint16_t last_ack; //fragment number represnting the last ack'd fragment (from rx) - head
    uint16_t curr_fragment; //fragment number of the current fragment being sent
    uint32_t timeout;

} SchedulerMetadata;

typedef struct _frame_ack_record {
    uint16_t last_ack; //last ack'd fragment recevied from the rx
    uint16_t total_frags; //total number of fragments associated with the sequence number
    uint16_t seq_num; //sequence number this ack corresponds to
} FrameAckRecord;

typedef struct _send_ack_metadata{
    uint8_t data[GENERIC_FRAG_ACK_DATA_SIZE];
    uint8_t sender_id;
} SendAckMetaData;

typedef struct _frame_compare {
    /**
     * @brief Uses aging based priority scheduling (linearly increasing priority with time)
     *
     * $P_f = B_f - A_f\alpha$
     *
     * - $P_f$ is the effective priority value (lower comes first)
     *
     * - $B_f$ is the base priority
     *
     * - $A_f$ is the age (amount of time the frame has waited in the queue)
     *
     * - $\alpha$ is the aging factor (rate at which a frame increases priority)
     *
     * @param a
     * @param b
     * @return true
     * @return false
     */
    bool operator()(const SchedulerMetadata& a, const SchedulerMetadata& b) const {
        int64_t now = esp_timer_get_time();
        double age_a = (now - a.enqueue_time_ns) / 1e6;
        double age_b = (now - a.enqueue_time_ns) / 1e6;

        // Base priorities: lower is higher priority
        double base_a = (IS_CONTROL_FRAME(a.header.type_flag)) ? 0.0 : 10.0;
        double base_b = (IS_CONTROL_FRAME(b.header.type_flag)) ? 0.0 : 10.0;

        // Aging coefficient (tune this)
        constexpr double aging_factor = 0.1;

        double effective_a = base_a - age_a * aging_factor;
        double effective_b = base_b - age_b * aging_factor;

        // If effective priority equal, fall back to enqueue time (FIFO)
        if (effective_a == effective_b) {
            return a.enqueue_time_ns > b.enqueue_time_ns;
        }

        // Return true if a has *lower* priority (so b stays on top)
        return effective_a < effective_b;
    }
} FrameCompare;

#endif //DATA_LINK
