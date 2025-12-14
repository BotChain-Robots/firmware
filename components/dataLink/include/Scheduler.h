#ifdef DATA_LINK
#include "Frames.h"
#include "esp_timer.h"
#include <cstdint>

//TODO: need to make a private send() to accept a Frame type. 
//the public send() is fine to keep but needs to be modified to simply create the Frame type based on the flags, data size, and args, and then push to the priority queue based on routing
//TODO: receive also needs to be updated, when receiving a frame not destined for that board; need to route that frame towards the actual destination (requires pushing that frame onto the priority queue)
//TODO: generic frames receive needs to be created (handle fragmenting, resend on corruption). ethe resend on corruption will be a lot of work as it requires ACK frames to be send and currently there is 0 support for ACK since we don't save any recently sent frames (can't resend)

#define SCHEDULER_MUTEX_WAIT 10 //max time duration to wait
#define SCHEDULER_PERIOD_MS 25

//Metadata representing the frame to be sent but is currently scheduled
typedef struct _frame_scheduler_metadata {
    FrameHeader header; //header of the frame
    uint16_t generic_frame_data_offset; //For data greater than MAX_CONTROL_DATA_LEN to keep track of fragment positions
    int64_t enqueue_time_ns; //when the frame has been first enqueued into the priority queue
    uint8_t* data; //dyanmically allocated memory - contains the actual data
    uint16_t len; // length of the actual data
} SchedulerMetadata;

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