//
// Created by Johnathon Slightham on 2025-06-30.
//

#include "AngleControlMessageBuilder.h"

namespace Flatbuffers {
    const Messaging::AngleControlMessage* AngleControlMessageBuilder::parse_angle_control_message(const uint8_t* buffer) {
        return flatbuffers::GetRoot<Messaging::AngleControlMessage>(buffer);
    }
}
