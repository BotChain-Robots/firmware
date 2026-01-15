// Justin Chow created file >:)

#include "MovementSetBuilder.h"
#include "SerializedMessage.h"

#ifdef MOVEMENTSETBUILDER

namespace Flatbuffers {
    SerializedMessage MovementSetBuilder::build_movement_set(const std::vector<flatbuffers::Offset<Movement::MovementEntry>>& set){

        builder_.Clear();

        const auto set_vector = builder_.CreateVector(set);

        const auto message = Movement::CreateMovementSet(
            builder_,
            static_cast<uint8_t>(set.size()),
            set_vector
        );

        builder_.Finish(message);

        return {builder_.GetBufferPointer(), builder_.GetSize()};
    }

    flatbuffers::Offset<Movement::MovementEntry> MovementSetBuilder::build_movement_entry(uint16_t board_id, uint8_t module_type, uint16_t value_action,
            const Movement::ConditionBlob& condition, uint8_t ack, uint16_t ack_ttl_ms, uint16_t post_delay_ms){
        return Movement::CreateMovementEntry(
            builder_,
            board_id,
            module_type,
            value_action,
            &condition,     
            ack,
            ack_ttl_ms,
            post_delay_ms
        );
    }

    Movement::ConditionBlob MovementSetBuilder::build_condition_blob(uint16_t value, uint8_t cond, uint8_t module_type, uint8_t in_use){
        return Movement::ConditionBlob(value, cond, module_type, in_use);
    }
}

#endif //MOVEMENTSETBUILDER
