#ifndef TOPOLOGYBUILDER
#define TOPOLOGYBUILDER

#include "SerializedMessage.h"
#include <vector>
#include "constants/datalink.h"
#include "constants/rmt.h"

#include "flatbuffers_generated/Topology_generated.h"
#include "flatbuffers/flatbuffers.h"

namespace Flatbuffers {
    class TopologyBuilder {
    public:
        TopologyBuilder() : builder_(RIP_MAX_ROUTES*(sizeof(Topology::NeighbourBlob) + sizeof(Topology::ChannelBoardConn)*MAX_CHANNELS)) {}

        SerializedMessage build_topology(const std::unordered_map<uint16_t, std::vector<Topology::ChannelBoardConn>>& topology_map);
        Topology::ChannelBoardConn build_connections(uint8_t channel, uint16_t board_id);
        
    private:
        flatbuffers::FlatBufferBuilder builder_;
    };
}

#endif //TOPOLOGYBUILDER