//
// Created by Johnathon Slightham on 2025-06-30.
//

#ifndef ANGLECONTROLMESSAGEBUILDER_H_
#define ANGLECONTROLMESSAGEBUILDER_H_

#include <string>
#include <vector>

#include "SerializedMessage.h"
#include "flatbuffers_generated/AngleControlMessage_generated.h"
#include "flatbuffers/flatbuffers.h"

namespace Flatbuffers {
    class AngleControlMessageBuilder {
    public:
        static const Messaging::AngleControlMessage* parse_angle_control_message(const uint8_t* buffer);
    };
}

#endif
