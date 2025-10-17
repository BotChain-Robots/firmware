//
// Created by Johnathon Slightham on 2025-06-30.
//

#ifndef TOPOLOGYMESSAGEBUILDER_H
#define TOPOLOGYMESSAGEBUILDER_H

#include <vector>

#include "SerializedMessage.h"
#include "flatbuffers_generated/TopologyMessage_generated.h"
#include "flatbuffers/flatbuffers.h"

namespace Flatbuffers {
    class TopologyMessageBuilder {
    public:
        TopologyMessageBuilder() : builder_(1024) {}

        SerializedMessage build_topology_message(
            uint8_t module_id,
            ModuleType module_type,
            const std::vector<uint8_t>& channel_to_module,
            const std::vector<int8_t>& orientation_to_module,
            Messaging::ConnectionType connection_type,
            uint8_t leader);

    private:
        flatbuffers::FlatBufferBuilder builder_;
    };
}

#endif //TOPOLOGYMESSAGEBUILDER_H
