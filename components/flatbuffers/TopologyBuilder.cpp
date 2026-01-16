#include "TopologyBuilder.h"
#ifdef TOPOLOGYBUILDER
namespace Flatbuffers{
    SerializedMessage TopologyBuilder::build_topology(const std::vector<flatbuffers::Offset<Topology::NeighbourBlob>>& topology){
        builder_.Clear();
        
        auto neighbours_vec = builder_.CreateVector(topology);
        auto topology_offset = Topology::CreateTopologyInfo(builder_, static_cast<uint16_t>(topology.size()), neighbours_vec);

        builder_.Finish(topology_offset);

        return {
            builder_.GetBufferPointer(),
            builder_.GetSize()
        };
    }

    flatbuffers::Offset<Topology::NeighbourBlob> TopologyBuilder::build_neighbour_info(uint16_t board_id, const std::vector<Topology::ChannelBoardConn>& connections){
        auto conn = builder_.CreateVectorOfStructs(connections);
        return Topology::CreateNeighbourBlob(builder_, board_id, static_cast<uint8_t>(connections.size()), conn);
    }

    Topology::ChannelBoardConn TopologyBuilder::build_connections(uint8_t channel, uint16_t board_id){
        return Topology::ChannelBoardConn(channel, board_id);
    }

}

#endif //TOPOLOGYBUILDER