//
// Created by Johnathon Slightham on 2025-06-30.
//

#include "TopologyMessageBuilder.h"

#include "SerializedMessage.h"

namespace Flatbuffers {
    SerializedMessage TopologyMessageBuilder::build_topology_message(
        const uint8_t module_id,
        const ModuleType module_type,
        const std::vector<uint8_t>& channel_to_module,
        const std::vector<int8_t>& orientation_to_module) {
        builder_.Clear();

        const auto orientation_to_module_vector = builder_.CreateVector(orientation_to_module);
        const auto channel_to_module_vector = builder_.CreateVector(channel_to_module);

        const auto message = Messaging::CreateTopologyMessage(
            builder_,
            module_id,
            module_type,
            channel_to_module.size(),
            channel_to_module_vector,
            orientation_to_module_vector
        );

        builder_.Finish(message);

        return {builder_.GetBufferPointer(), builder_.GetSize()};
    }

    const Messaging::TopologyMessage* TopologyMessageBuilder::parse_topology_message(const uint8_t* buffer) {
        return flatbuffers::GetRoot<Messaging::TopologyMessage>(buffer);
    }
}
