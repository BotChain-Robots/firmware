#include <iostream>

#include "AngleControlMessageBuilder.h"
#include "CommunicationRouter.h"
#include "MPIMessageBuilder.h"
#include "OrientationDetection.h"
#include "PtrQueue.h"
#include "Tables.h"
#include "constants/module.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "include/wireless/WifiManager.h"
#include "include/wireless/mDNSDiscoveryService.h"

#define TAG "CommunicationRouter"

CommunicationRouter::~CommunicationRouter() { vTaskDelete(m_router_thread); }

// todo: we really need to change all char to uint8_t everywhere
// todo: get rid of copying going on, need to pass around sharedptrs/uniqueptrs

[[noreturn]] void CommunicationRouter::router_thread(void *args) {
  const auto that = static_cast<CommunicationRouter *>(args);

  while (true) {
    const auto buffer = that->m_tcp_rx_queue->dequeue();
    ESP_LOGD(TAG, "Got message from TCP");
    that->route(buffer->data(), buffer->size());
  }
}

[[noreturn]] void CommunicationRouter::link_layer_thread(void *args) {
  const auto that = static_cast<CommunicationRouter *>(args);

  while (true) {
    if (std::chrono::system_clock::now() - that->m_last_leader_updated >
        std::chrono::seconds(15)) {
      that->m_last_leader_updated = std::chrono::system_clock::now();
      ESP_LOGI(TAG, "Updating leader");
      that->update_leader();
    }

    for (uint8_t channel = 0;
         channel <
         MODULE_TO_NUM_CHANNELS_MAP[that->m_config_manager.get_module_type()];
         channel++) {
      uint16_t frame_size = 0;
      FrameHeader frame_header{};
      if (ESP_OK != that->m_data_link_manager->async_receive_info(
                        &frame_size, &frame_header, channel) ||
          0 == frame_size) {
        continue;
      }

      std::vector<uint8_t> data{};
      data.resize(frame_size);
      that->m_data_link_manager->async_receive(data.data(), frame_size,
                                               &frame_header, channel);
      that->route(data.data(), frame_size);
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

int CommunicationRouter::send_msg(char *buffer, const size_t length) const {
  ESP_LOGD(TAG, "Got message from application");
  route(reinterpret_cast<uint8_t *>(buffer), length);
  return 0;
}

void CommunicationRouter::update_leader() {
  RIPRow_public table[RIP_MAX_ROUTES];
  size_t table_size = RIP_MAX_ROUTES;
  this->m_data_link_manager->get_routing_table(table, &table_size);

  // Leader election (just get the highest id in rip)
  std::vector<int> connected_module_ids;
  uint8_t max = m_module_id;
  for (int i = 0; i < table_size; i++) {
    const auto id = table[i].info.board_id;
    connected_module_ids.emplace_back(id);
    if (id > max) { // todo: change this to be correct
      max = id;
    }
  }

  // Leader has changed, we may need to change PC connection state
  if (this->m_leader != max) {
    if (max == m_module_id) {
      m_pc_connection->connect();
      m_lossless_server->startup();
      m_lossy_server->startup();
    } else if (this->m_leader == m_module_id) {
      m_pc_connection->disconnect();
      m_lossless_server->shutdown();
      m_lossy_server->shutdown();
    }
  }

  this->m_leader = max;

  if (this->m_leader == m_module_id) {
    this->m_discovery_service->set_connected_boards(connected_module_ids);
  }
}

void CommunicationRouter::route(uint8_t *buffer, const size_t length) const {
  flatbuffers::Verifier verifier(buffer, length);
  // This could be moved to just be called on wireline data to save cpu cycles.
  if (bool ok = Messaging::VerifyMPIMessageBuffer(verifier); !ok) {
    ESP_LOGW(TAG, "route: got an invalid MPI message, disregarding");
    return;
  }

  if (const auto &mpi_message =
          Flatbuffers::MPIMessageBuilder::parse_mpi_message(buffer);
      mpi_message->destination() == m_module_id) {
    this->m_rx_callback(reinterpret_cast<char *>(buffer), 512);
  } else if (mpi_message->destination() == PC_ADDR &&
             this->m_leader == m_module_id) {
    if (mpi_message->is_durable()) {
      this->m_lossless_server->send_msg(reinterpret_cast<char *>(buffer), 512);
    } else {
      this->m_lossy_server->send_msg(reinterpret_cast<char *>(buffer), 512);
    }
  } else if (mpi_message->destination() == PC_ADDR) {
    this->m_data_link_manager->send(this->m_leader, buffer, length,
                                    FrameType::MOTOR_TYPE, 0);
  } else {
    this->m_data_link_manager->send(mpi_message->destination(), buffer, length,
                                    FrameType::MOTOR_TYPE, 0);
  }
}

std::pair<std::vector<uint8_t>, std::vector<Orientation>>
CommunicationRouter::get_physically_connected_modules() const {
  std::vector<RIPRow_public> table;
  table.resize(RIP_MAX_ROUTES);
  size_t table_size = RIP_MAX_ROUTES * sizeof(RIPRow_public);
  m_data_link_manager->get_routing_table(table.data(), &table_size);

  std::vector<uint8_t> connected_module_ids;
  std::vector<Orientation> connected_module_orientations;
  connected_module_ids.resize(MAX_WIRED_CONNECTIONS);
  connected_module_orientations.resize(MAX_WIRED_CONNECTIONS);

  for (int i = 0; i < MAX_WIRED_CONNECTIONS; i++) {
    connected_module_ids[i] = 0; // this is not the PC ID here, marking as nc.
  }

  for (int i = 0; i < table_size; i++) {
    if (table[i].info.hops == 1 && table[i].channel < MAX_WIRED_CONNECTIONS) {
      connected_module_ids[table[i].channel] = table[i].info.board_id;
    }
  }

  if (const auto id = connected_module_ids[0]; 0 == id) {
    connected_module_orientations[0] = Orientation_Deg0;
  } else {
    connected_module_orientations[0] = OrientationDetection::get_orientation(0);
  }

  return {connected_module_ids, connected_module_orientations};
}

[[nodiscard]] uint8_t CommunicationRouter::get_leader() const {
  return this->m_leader;
}
