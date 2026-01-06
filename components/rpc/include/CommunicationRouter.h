//
// Created by Johnathon Slightham on 2025-05-25.
//

// todo: this class is getting a bit large

#ifndef COMMUNICATIONROUTER_H
#define COMMUNICATIONROUTER_H

#include <chrono>
#include <functional>
#include <memory>

#include "CommunicationFactory.h"
#include "ConfigManager.h"
#include "DataLinkManager.h"
#include "IDiscoveryService.h"
#include "OrientationDetection.h"
#include "PtrQueue.h"
#include "constants/module.h"
#include "wireless/TCPServer.h"
#include "wireless/WifiManager.h"

class CommunicationRouter {

public:
  explicit CommunicationRouter(
      const std::function<void(char *, int)> &rx_callback)
      : m_tcp_rx_queue(std::make_shared<PtrQueue<std::vector<uint8_t>>>(10)),
        m_rx_callback(rx_callback),
        m_config_manager(ConfigManager::get_instance()),
        m_pc_connection(CommunicationFactory::create_connection_manager(
            m_config_manager.get_communication_method())),
        m_lossless_server(CommunicationFactory::create_lossless_server(
            m_config_manager.get_communication_method(), m_tcp_rx_queue)),
        m_lossy_server(CommunicationFactory::create_lossy_server(
            m_config_manager.get_communication_method(), m_tcp_rx_queue)),
        m_data_link_manager(std::make_unique<DataLinkManager>(
            m_config_manager.get_module_id(),
            MODULE_TO_NUM_CHANNELS_MAP[m_config_manager.get_module_type()])),
        m_module_id(m_config_manager.get_module_id()),
        m_last_leader_updated(std::chrono::system_clock::now()),
        m_discovery_service(CommunicationFactory::create_discovery_service(
            m_config_manager.get_communication_method())) {
    OrientationDetection::init();
    update_leader();

    xTaskCreate(router_thread, "router", 4096, this, 3, &this->m_router_thread);
    xTaskCreate(link_layer_thread, "router_rmt", 4096, this, 3,
                &this->m_link_layer_thread);
  }

  ~CommunicationRouter();

  [[noreturn]] static void router_thread(void *args);
  [[noreturn]] static void link_layer_thread(void *args);
  int send_msg(char *buffer, size_t length) const;
  void update_leader();
  void route(uint8_t *buffer, size_t length) const;
  [[nodiscard]] std::pair<std::vector<uint8_t>, std::vector<Orientation>>
  get_physically_connected_modules() const;
  [[nodiscard]] uint8_t get_leader() const;

  // todo: does this really need to be here (so i can access from thread)?
  std::shared_ptr<PtrQueue<std::vector<uint8_t>>>
      m_tcp_rx_queue; // todo: this should probably be thread safe
  std::function<void(char *, int)> m_rx_callback;

private:
  TaskHandle_t m_router_thread = nullptr;
  TaskHandle_t m_link_layer_thread = nullptr;
  ConfigManager &m_config_manager;
  std::unique_ptr<IConnectionManager> m_pc_connection;
  std::unique_ptr<IRPCServer> m_lossless_server;
  std::unique_ptr<IRPCServer> m_lossy_server;
  std::unique_ptr<DataLinkManager> m_data_link_manager;
  uint8_t m_leader = 0;
  uint8_t m_module_id;
  std::chrono::time_point<std::chrono::system_clock> m_last_leader_updated;
  std::unique_ptr<IDiscoveryService> m_discovery_service;
};

#endif // COMMUNICATIONROUTER_H
