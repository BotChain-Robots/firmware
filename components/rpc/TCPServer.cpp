#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "TCPServer.h"

#include <bits/shared_ptr_base.h>

#include "constants/tcp.h"

// todo: - add message routing to correct client
//       - authenticate (don't just return true from the auth function)
//       - tx from board

TCPServer::TCPServer(const int port, QueueHandle_t rx_queue) {
    this->m_port = port;
    this->m_mutex = xSemaphoreCreateMutex();
    this->m_clients = std::unordered_set<int>();
    this->m_task = nullptr;
    this->m_rx_task = nullptr;
    this->m_tx_task = nullptr;
    this->m_rx_queue = rx_queue;
    this->m_server_sock = 0;

    xTaskCreate(tcp_server_task, "tcp_accept_server", 2048, this, 5, &this->m_task);
    xTaskCreate(socket_monitor_thread, "tcp_rx", 4096, this, 5, &this->m_rx_task);
}

TCPServer::~TCPServer() {
    vTaskDelete(this->m_task);
    vTaskDelete(this->m_rx_task);
    vTaskDelete(this->m_tx_task);
    vSemaphoreDelete(this->m_mutex);
}

[[noreturn]] void TCPServer::tcp_server_task(void *args) {
    constexpr int keepAlive = 1;
    constexpr int keepIdle = KEEPALIVE_IDLE;
    constexpr int keepInterval = KEEPALIVE_INTERVAL;
    constexpr int keepCount = KEEPALIVE_COUNT;

    const auto that = static_cast<TCPServer*>(args);

    while (true) {
        printf("Attempting to start TCP Server on port %d", that->m_port);

        that->m_server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (that->m_server_sock < 0) {
            printf("Unable to create TCP socket: errno %d\n", errno);
            vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
            continue;
        }

        constexpr int opt = 1;
        setsockopt(that->m_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        printf("Socket created\n");

        sockaddr_in server_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(that->m_port),
            .sin_addr = {
                .s_addr = htonl(INADDR_ANY),
                },
        };

        int err = bind(that->m_server_sock, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr));
        if (0 != err) {
            printf("Socket unable to bind: errno %d\n", errno);
            close(that->m_server_sock);
            that->m_server_sock = -1;
            vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
            continue;
        }

        printf("Socket bound to port %d\n", that->m_port);

        err = listen(that->m_server_sock, TCP_DEFAULT_LISTEN_BACKLOG);
        if (0 != err) {
            printf("Error occurred during TCP listen: errno %d\n", errno);
            close(that->m_server_sock);
            that->m_server_sock = -1;
            vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
            continue;
        }

        while (is_network_connected()) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int client_sock = accept(that->m_server_sock, reinterpret_cast<struct sockaddr *>(&client_addr), &addr_len);
            if (client_sock < 0) {
                printf("Unable to accept TCP connection: errno %d\n", errno);
                continue;
            }

            err = that->authenticate_client(client_sock);
            if (0 != err) {
                printf("Client failed authentication\n");
            }

            setsockopt(client_sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
            setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
            setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
            setsockopt(client_sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

            xSemaphoreTake(that->m_mutex, portMAX_DELAY);
            that->m_clients.emplace(client_sock);
            xSemaphoreGive(that->m_mutex);
        }

        close(that->m_server_sock);
        vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
    }
}

[[noreturn]] void TCPServer::socket_monitor_thread(void *args) {
    const auto that = static_cast<TCPServer *>(args);

    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;

        xSemaphoreTake(that->m_mutex, portMAX_DELAY);
        for (const auto sock : that->m_clients) {
            FD_SET(sock, &readfds);
            if (sock > max_fd) max_fd = sock;
        }
        xSemaphoreGive(that->m_mutex);

        timeval timeout = {1, 0}; // 1 second timeout
        int ret = select(max_fd + 1, &readfds, nullptr, nullptr, &timeout);

        if (ret > 0) {
            xSemaphoreTake(that->m_mutex, portMAX_DELAY);
            std::vector<int> to_remove;
            for (int sock : that->m_clients) {
                if (FD_ISSET(sock, &readfds)) {
                    // Handle socket
                    char buffer[512];
                    int len = recv(sock, buffer, sizeof(buffer) - 1, 0); // temp: for the null terminator

                    if (len < 0) {
                        printf("Error occurred during receiving: errno %d\n", errno);
                    } else if (0 == len) {
                        printf("Connection closed\n");
                        close(sock);
                        to_remove.emplace_back(sock);
                    } else {
                        printf("TCP Server Received %d bytes\n", len);
                        xQueueSendToBack(that->m_rx_queue, buffer, 0);
                    }
                }
            }

            for (const auto r : to_remove) {
                that->m_clients.erase(r);
            }

            xSemaphoreGive(that->m_mutex);
        }
    }
}

bool TCPServer::is_network_connected() {
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    if (netif != nullptr && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        return true;
    }

    if (0 != ip_info.ip.addr) {
        return true;
    }

    netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    if (netif != nullptr && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        return true;
    }

    if (0 != ip_info.ip.addr) {
        return true;
    }

    return false;
}

bool TCPServer::authenticate_client(int sock) {
    // todo: authentication (wait for a passphrase from the client)
    return 0;
}
