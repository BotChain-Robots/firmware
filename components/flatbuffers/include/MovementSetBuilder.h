// Justin Chow created file >:)

#ifndef MOVEMENTSETBUILDER
#define MOVEMENTSETBUILDER

#define MAX_MOVEMENTS_IN_SET 255 //max number movements in `MovementEntry.num_movements` can hold

#include <vector>

#include "SerializedMessage.h"
#include "flatbuffers_generated/Movement_generated.h"
#include "flatbuffers/flatbuffers.h"

struct MovementEntryData {
    uint16_t board_id;
    uint8_t module_type;
    uint16_t value_action;
    Movement::ConditionBlob condition; // struct is fine to reuse
    uint8_t ack;
    uint16_t ack_ttl_ms;
    uint16_t post_delay_ms;

    bool operator==(const MovementEntryData& other) const {
    return board_id == other.board_id &&
            module_type == other.module_type &&
            value_action == other.value_action &&
            condition.value() == other.condition.value() &&
            condition.cond() == other.condition.cond() &&
            condition.module_type() == other.condition.module_type() &&
            condition.in_use() == other.condition.in_use() &&
            ack == other.ack &&
            ack_ttl_ms == other.ack_ttl_ms &&
            post_delay_ms == other.post_delay_ms;
    }
};


namespace Flatbuffers {
    class MovementSetBuilder {
    public:
        MovementSetBuilder() : builder_(MAX_MOVEMENTS_IN_SET*sizeof(Movement::MovementEntry)) {}

        SerializedMessage build_movement_set(const std::unordered_map<uint8_t, std::vector<MovementEntryData>>& input);
        Movement::ConditionBlob build_condition_blob(uint16_t value, uint8_t cond, uint8_t module_type, uint8_t in_use);

    private:
        flatbuffers::FlatBufferBuilder builder_;
    };
}

#endif //MOVEMENTSETBUILDER
