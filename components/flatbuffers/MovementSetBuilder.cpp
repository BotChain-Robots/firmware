// Justin Chow created file >:)

#include "MovementSetBuilder.h"
#include "SerializedMessage.h"

#ifdef MOVEMENTSETBUILDER

namespace Flatbuffers {
    SerializedMessage MovementSetBuilder::build_movement_set(const std::unordered_map<uint8_t, std::vector<MovementEntryInput>>& input){
        builder_.Clear();

        std::vector<flatbuffers::Offset<Movement::MovementVector>> map_entries;
        map_entries.reserve(input.size());

        // 1. Collect and sort keys (DO NOT sort offsets)
        std::vector<uint8_t> keys;
        keys.reserve(input.size());

        for (const auto& [key, _] : input) {
        keys.push_back(key);
        }

        std::sort(keys.begin(), keys.end());

        // 2. Build MovementVector entries in key order
        for (uint8_t key : keys) {
        const auto& entries = input.at(key);

        std::vector<flatbuffers::Offset<Movement::MovementEntry>> fb_entries;
        fb_entries.reserve(entries.size());

        for (const auto& e : entries) {
            fb_entries.push_back(
                Movement::CreateMovementEntry(
                    builder_,
                    e.board_id,
                    e.module_type,
                    e.value_action,
                    &e.condition,
                    e.ack,
                    e.ack_ttl_ms,
                    e.post_delay_ms
                )
            );
        }

        auto movements_vector = builder_.CreateVector(fb_entries);

        map_entries.push_back(
            Movement::CreateMovementVector(
                builder_,
                key,
                movements_vector
            )
        );
        }

        // 3. Build the map vector
        auto map_vector = builder_.CreateVector(map_entries);

        auto message = Movement::CreateMovementSet(
        builder_,
        map_vector
        );

        builder_.Finish(message);

        return { builder_.GetBufferPointer(), builder_.GetSize() };
    }

    Movement::ConditionBlob MovementSetBuilder::build_condition_blob(uint16_t value, uint8_t cond, uint8_t module_type, uint8_t in_use){
        return Movement::ConditionBlob(value, cond, module_type, in_use);
    }

}

#endif //MOVEMENTSETBUILDER
