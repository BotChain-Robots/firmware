//
// Created by Johnathon Slightham on 2025-05-25.
//

#ifndef COMMUNICATIONROUTER_H
#define COMMUNICATIONROUTER_H

#include <ConfigManager.h>
#include <functional>
#include <memory>
#include <chrono>
#include <WifiManager.h>

#include "freertos/FreeRTOS.h"

#include "TCPServer.h"
#include "DataLinkManager.h"
#include "constants/tcp.h"
#include "constants/module.h"

#include"PtrQueue.h"

class CommunicationRouter {

    class link_layer_thread_params {
    public:
        link_layer_thread_params(CommunicationRouter* router, const uint8_t channel) : router(router), channel(channel) {};
        CommunicationRouter *router;
        uint8_t channel;
    };

public:
    CommunicationRouter(const std::function<void(char*, int)> &rx_callback, std::unique_ptr<WifiManager>&& pc_connection)
        : m_tcp_rx_queue(std::make_shared<PtrQueue<std::vector<uint8_t>>>(10)),
            m_rx_callback(rx_callback),
            m_tcp_server(std::make_unique<TCPServer>(TCP_PORT, m_tcp_rx_queue)),
            m_data_link_manager(std::make_unique<DataLinkManager>(ConfigManager::get_module_id(), MODULE_TO_NUM_CHANNELS_MAP[ConfigManager::get_module_type()])),
            m_pc_connection(std::move(pc_connection)),
            m_module_id(ConfigManager::get_module_id()),
            m_last_leader_updated(std::chrono::system_clock::now()){
        update_leader();

        xTaskCreate(router_thread, "communication_router", 2048, this, 3, &this->m_router_thread);

        const auto num_channels = MODULE_TO_NUM_CHANNELS_MAP[ConfigManager::get_module_type()];
        this->m_link_layer_threads.resize(num_channels);
        for (uint8_t i = 0; i < num_channels; i++) {
            auto *params = new link_layer_thread_params(this, i);
            xTaskCreate(link_layer_thread, "communication_router_rmt", 3096, params, 3, &this->m_link_layer_threads[i]);
        }
    }

    ~CommunicationRouter();

    [[noreturn]] static void router_thread(void *args);
    [[noreturn]] static void link_layer_thread(void *args);

    int send_msg(char* buffer, size_t length) const;

    void update_leader();

    void route(uint8_t *buffer, size_t length) const;

    // todo: does this really need to be here (so i can access from thread)?
    std::shared_ptr<PtrQueue<std::vector<uint8_t>>> m_tcp_rx_queue;
    std::function<void(char*, int)> m_rx_callback;
private:
    TaskHandle_t m_router_thread;
    std::vector<TaskHandle_t> m_link_layer_threads;
    std::unique_ptr<TCPServer> m_tcp_server;
    std::unique_ptr<DataLinkManager> m_data_link_manager;
    std::unique_ptr<WifiManager> m_pc_connection; // todo: change to dependency inject
    uint8_t m_leader = 0;
    uint8_t m_module_id;
    std::chrono::time_point<std::chrono::system_clock> m_last_leader_updated;
};

#endif //COMMUNICATIONROUTER_H
