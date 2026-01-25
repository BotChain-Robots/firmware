#include "TopologyBuilder.h"
#ifdef TOPOLOGYBUILDER
namespace Flatbuffers{
    SerializedMessage TopologyBuilder::build_topology(const std::unordered_map<uint16_t, std::vector<Topology::ChannelBoardConn>>& topology_map) {
        builder_.Clear();

        std::vector<flatbuffers::Offset<Topology::NeighbourBlob>> neighbours;
        neighbours.reserve(topology_map.size());

        for (const auto& [board_id, connections] : topology_map) {
            auto conn_vec = builder_.CreateVectorOfStructs(connections);
            auto blob = Topology::CreateNeighbourBlob(
                builder_,
                board_id,
                static_cast<uint8_t>(connections.size()),
                conn_vec
            );

            neighbours.push_back(blob);
        }

        auto neighbours_vec = builder_.CreateVector(neighbours);
        auto topology_offset = Topology::CreateTopologyInfo(
            builder_,
            static_cast<uint16_t>(neighbours.size()),
            neighbours_vec
        );

        builder_.Finish(topology_offset);

        return { builder_.GetBufferPointer(), builder_.GetSize() };
    }

    flatbuffers::Offset<Topology::NeighbourBlob> TopologyBuilder::build_neighbour_info(
        uint16_t board_id,
        const std::vector<Topology::ChannelBoardConn>& connections
    ) {
        auto conn_vec = builder_.CreateVectorOfStructs(connections);

        return Topology::CreateNeighbourBlob(
            builder_,
            board_id,
            static_cast<uint8_t>(connections.size()),
            conn_vec
        );
    }

    Topology::ChannelBoardConn TopologyBuilder::build_connections(uint8_t channel, uint16_t board_id){
        return Topology::ChannelBoardConn(channel, board_id);
    }

}

#endif //TOPOLOGYBUILDER