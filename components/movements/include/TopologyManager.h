#include "MovementManager.h"

#ifdef MOVEMENTS
#include "nvs_flash.h"
#include <unordered_map>
#include <unordered_set>

// move these to constants header file?
#include "RMTManager.h"
#include "DataLinkManager.h"
#include "Frames.h"
#include "TopologyBuilder.h"
#include <vector>
#include <stack>
#include <algorithm>

class TopologyManager {
    public:
        TopologyManager();
        ~TopologyManager();
        esp_err_t add_board_to_topology(std::vector<std::pair<uint8_t, uint16_t>>& connections, uint16_t curr_board_id);
        esp_err_t remove_board_from_topology(uint16_t board_id);
        esp_err_t get_board_in_topology(std::vector<std::pair<uint8_t, uint16_t>>& connections, uint16_t curr_board_id);
        esp_err_t verify_topology();
        esp_err_t get_curr_topology(std::unordered_map<uint16_t, std::vector<std::pair<uint8_t, uint16_t>>>& topology);
        esp_err_t write_nvs_topology();
        esp_err_t get_nvs_topology(std::unordered_map<uint16_t, std::vector<Topology::ChannelBoardConn>>& topology);

    private:
        std::unordered_map<uint16_t, std::vector<Topology::ChannelBoardConn>> topology;
        nvs_handle_t handle;
        bool ready = false;
        Flatbuffers::TopologyBuilder builder;
};

#endif //MOVEMENTS