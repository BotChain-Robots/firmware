#include <memory>

#include "bits/shared_ptr_base.h"
#include "constants/app_comms.h"
#include "constants/tcp.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "sys/param.h"
#include "wireless/TCPServer.h"

#define TAG "TCPServer"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

// todo: - add message routing to correct client
//       - authenticate (don't just return true from the auth function)
//       - tx from board

TCPServer::TCPServer(const int port,
                     const std::shared_ptr<PtrQueue<std::vector<uint8_t>>> &rx_queue) {
    this->m_port = port;
    this->m_mutex = xSemaphoreCreateMutex();
    this->m_clients = std::unordered_set<int>();
    this->m_task = nullptr;
    this->m_rx_task = nullptr;
    this->m_rx_queue = rx_queue;
    this->m_server_sock = 0;
}

TCPServer::~TCPServer() {
    this->shutdown();
    vSemaphoreDelete(this->m_mutex);
}

void TCPServer::startup() {
    ESP_LOGI(TAG, "Starting TCP server on port %d", this->m_port);
    if (nullptr != this->m_task || nullptr != this->m_rx_task) {
        ESP_LOGW(TAG, "Attempted to start TCP server when already started, "
                      "ignoring start request");
        return;
    }

    xTaskCreate(tcp_server_task, "tcp_accept_server", 3072, this, 5, &this->m_task);
    xTaskCreate(socket_monitor_thread, "tcp_rx", 4096, this, 5, &this->m_rx_task);
}

void TCPServer::shutdown() {
    ESP_LOGI(TAG, "Shutting down TCP server");
    if (nullptr != this->m_task) {
        vTaskDelete(this->m_task);
        close(this->m_server_sock);
        this->m_task = nullptr;
        this->m_server_sock = -1;
    }

    if (nullptr != this->m_rx_task) {
        vTaskDelete(this->m_rx_task);
        for (const auto sock : this->m_clients) {
            close(sock);
        }
        this->m_rx_task = nullptr;
        this->m_clients.clear();
    }
}

[[noreturn]] void TCPServer::tcp_server_task(void *args) {
    constexpr int keepAlive = 1;
    constexpr int keepIdle = KEEPALIVE_IDLE;
    constexpr int keepInterval = KEEPALIVE_INTERVAL;
    constexpr int keepCount = KEEPALIVE_COUNT;

    const auto that = static_cast<TCPServer *>(args);

    while (true) {
        ESP_LOGI(TAG, "Attempting to start TCP Server on port %d", that->m_port);

        that->m_server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (that->m_server_sock < 0) {
            ESP_LOGE(TAG, "Unable to create TCP socket: errno %d\n", errno);
            vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
            continue;
        }

        constexpr int opt = 1;
        setsockopt(that->m_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        ESP_LOGI(TAG, "Socket created");

        sockaddr_in server_addr = {
            .sin_family = AF_INET,
            .sin_port = htons(that->m_port),
            .sin_addr =
                {
                    .s_addr = htonl(INADDR_ANY),
                },
        };

        int err = bind(that->m_server_sock, reinterpret_cast<struct sockaddr *>(&server_addr),
                       sizeof(server_addr));
        if (0 != err) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d\n", errno);
            close(that->m_server_sock);
            that->m_server_sock = -1;
            vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "Socket bound to port %d\n", that->m_port);

        err = listen(that->m_server_sock, TCP_DEFAULT_LISTEN_BACKLOG);
        if (0 != err) {
            ESP_LOGE(TAG, "Error occurred during TCP listen: errno %d\n", errno);
            close(that->m_server_sock);
            that->m_server_sock = -1;
            vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
            continue;
        }

        while (is_network_connected()) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            int client_sock = accept(that->m_server_sock,
                                     reinterpret_cast<struct sockaddr *>(&client_addr), &addr_len);
            if (client_sock < 0) {
                ESP_LOGE(TAG, "Unable to accept TCP connection: errno %d\n", errno);
                continue;
            }

            err = that->authenticate_client(client_sock);
            if (0 != err) {
                ESP_LOGE(TAG, "Client failed authentication\n");
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
        vTaskDelay(0); // Avoid starving other threads

        fd_set readfds;
        FD_ZERO(&readfds);
        int max_fd = -1;

        xSemaphoreTake(that->m_mutex, portMAX_DELAY);
        if (that->m_clients.size() < 1) {
            vTaskDelay(NO_CLIENT_SLEEP_MS / portTICK_PERIOD_MS);
        }
        for (const auto sock : that->m_clients) {
            FD_SET(sock, &readfds);
            if (sock > max_fd)
                max_fd = sock;
        }
        xSemaphoreGive(that->m_mutex);

        timeval timeout = {.tv_sec = 1, .tv_usec = 0}; // 1s timeout
        int ret = select(max_fd + 1, &readfds, nullptr, nullptr, &timeout);

        vTaskDelay(0); // Avoid starving other threads

        if (ret > 0) {
            xSemaphoreTake(that->m_mutex, portMAX_DELAY);
            std::vector<int> to_remove;
            for (int sock : that->m_clients) {
                vTaskDelay(0); // Avoid starving other threads
                if (FD_ISSET(sock, &readfds)) {

                    uint32_t msg_size = 0;
                    if (int len = recv(sock, &msg_size, 4, MSG_WAITALL); len < 0) {
                        ESP_LOGE(TAG, "Error occurred during receiving msg length: errno %d\n",
                                 errno);
                        to_remove.emplace_back(sock);
                        continue;
                    } else if (0 == len) {
                        ESP_LOGI(TAG, "TCP Connection closed when receiving msg length\n");
                        close(sock);
                        to_remove.emplace_back(sock);
                        continue;
                    }

                    if (msg_size < 1 || msg_size > MAX_RX_BUFFER_SIZE) {
                        continue;
                    }

                    auto buffer = std::make_unique<std::vector<uint8_t>>();
                    buffer->resize(MIN(MAX_RX_BUFFER_SIZE, msg_size));

                    if (int len = recv(sock, buffer->data(), msg_size, MSG_WAITALL); len < 0) {
                        ESP_LOGE(TAG, "Error occurred during receiving: errno %d\n", errno);
                        to_remove.emplace_back(sock);
                    } else if (0 == len) {
                        ESP_LOGI(TAG, "TCP Connection closed\n");
                        close(sock);
                        to_remove.emplace_back(sock);
                    } else {
                        ESP_LOGD(TAG, "TCP Server Received %d bytes\n", len);
                        buffer->resize(len);
                        that->m_rx_queue->enqueue(std::move(buffer));
                    }
                }
            }

            for (const auto r : to_remove) {
                that->m_clients.erase(r);
                close(r);
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
    // todo: authentication (key?)
    return 0;
}

int TCPServer::send_msg(char *buffer, const uint32_t length) const {
    if (!is_network_connected()) {
        return -1;
    }

    for (const auto client_sock : m_clients) {
        send(client_sock, &length, 4, 0);
        send(client_sock, buffer, length, 0);
    }

    return 0;
}
