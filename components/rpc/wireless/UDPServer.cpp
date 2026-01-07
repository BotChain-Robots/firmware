#include <cstring>
#include <memory>

#include "bits/shared_ptr_base.h"
#include "constants/app_comms.h"
#include "constants/udp.h"
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
#include "wireless/UDPServer.h"

#define TAG "UDPServer"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

// todo: - authenticate

UDPServer::UDPServer(
    const int rx_port, const int tx_port,
    const std::shared_ptr<PtrQueue<std::vector<uint8_t>>> &rx_queue) {
  this->m_rx_port = rx_port;
  this->m_tx_port = tx_port;
  this->m_rx_task = nullptr;
  this->m_rx_queue = rx_queue;
  this->m_rx_server_sock = 0;
  this->m_tx_server_sock = 0;
}

UDPServer::~UDPServer() { this->shutdown(); }

void UDPServer::startup() {
  ESP_LOGI(TAG, "Starting UDP server on port %d", this->m_rx_port);
  if (nullptr != this->m_rx_task) {
    ESP_LOGW(TAG, "Attempted to start UDP server when already started, "
                  "ignoring start request");
    return;
  }

  xTaskCreate(socket_monitor_thread, "udp_rx", 4096, this, 5, &this->m_rx_task);
}

void UDPServer::shutdown() {
  ESP_LOGI(TAG, "Shutting down UDP server");
  if (nullptr != this->m_rx_task) {
    vTaskDelete(this->m_rx_task);
    close(this->m_rx_server_sock);
    close(this->m_tx_server_sock);
    this->m_rx_task = nullptr;
    this->m_rx_server_sock = -1;
    this->m_tx_server_sock = -1;
  }
}

[[noreturn]] void UDPServer::socket_monitor_thread(void *args) {
  const auto that = static_cast<UDPServer *>(args);

  while (true) {
    ESP_LOGI(TAG, "Attempting to start UDP Server on %d", that->m_rx_port);

    if (!is_network_connected()) {
      ESP_LOGW(TAG, "Network is disconnected");
      vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
      continue;
    }

    sockaddr_in saddr = {0};
    sockaddr_in from_addr = {0};

    that->m_rx_server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (that->m_rx_server_sock == -1) {
      ESP_LOGE(TAG, "Create UDP socket fail");
      vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
      continue;
    }

    that->m_tx_server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (that->m_tx_server_sock < 0) {
      ESP_LOGE(TAG, "Unable to create UDP tx socket: errno %d", errno);
      close(that->m_rx_server_sock);
      that->m_rx_server_sock = -1;
      vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
      continue;
    }

    int reuse = 1;
    if (setsockopt(that->m_rx_server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse,
                   sizeof(reuse)) < 0) {
      ESP_LOGE(TAG, "Failed to set SO_REUSEADDR. Error %d", errno);
      close(that->m_rx_server_sock);
      close(that->m_tx_server_sock);
      that->m_rx_server_sock = -1;
      that->m_tx_server_sock = -1;
      vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
      continue;
    }

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(that->m_rx_port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    int ret = bind(that->m_rx_server_sock, (struct sockaddr *)&saddr,
                   sizeof(struct sockaddr_in));
    if (ret < 0) {
      ESP_LOGE(TAG, "Failed to bind socket. Error %d", errno);
      close(that->m_rx_server_sock);
      close(that->m_tx_server_sock);
      that->m_rx_server_sock = -1;
      that->m_tx_server_sock = -1;
      vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
      continue;
    }

    struct ip_mreq imreq = {};
    imreq.imr_multiaddr.s_addr = inet_addr(RECV_MCAST);
    imreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(that->m_rx_server_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &imreq, sizeof(struct ip_mreq)) < 0) {
      ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP. Error %d", errno);
      close(that->m_rx_server_sock);
      close(that->m_tx_server_sock);
      that->m_rx_server_sock = -1;
      that->m_tx_server_sock = -1;
      vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
      continue;
    }

    uint32_t msg_size;
    while (is_network_connected()) {
      auto buffer = std::make_unique<std::vector<uint8_t>>();
      buffer->resize(MAX_RX_BUFFER_SIZE + 4);

      if (int len = recvfrom(that->m_rx_server_sock, buffer->data(),
                             MAX_RX_BUFFER_SIZE, 0, nullptr, nullptr);
          len < 0) {
        ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
      } else if (len < 4 || len > MAX_RX_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Got illegal message size");
      } else {
        msg_size = *reinterpret_cast<uint32_t *>(buffer->data());
        if (msg_size > len - 4) {
          ESP_LOGW(TAG, "Message size incorrect");
          continue;
        }
        buffer->erase(buffer->begin(), buffer->begin() + 4); // todo: copying
        buffer->resize(msg_size);
        that->m_rx_queue->enqueue(std::move(buffer));
      }
    }

    ESP_LOGW(TAG, "Network disconnected");
    close(that->m_tx_server_sock);
    that->m_tx_server_sock = -1;
    vTaskDelay(SLEEP_AFTER_FAIL_MS / portTICK_PERIOD_MS);
  }
}

bool UDPServer::is_network_connected() {
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

bool UDPServer::authenticate_client(int sock) {
  // todo: authentication (key?)
  return 0;
}

int UDPServer::send_msg(char *buffer, const uint32_t length) const {
  if (!is_network_connected() || m_tx_server_sock == -1) {
    return -1;
  }

  sockaddr_in mcast_dest = {
      .sin_family = AF_INET,
      .sin_port = htons(m_tx_port),
      .sin_addr = {.s_addr = inet_addr(SEND_MCAST)},
  };

  uint32_t size = length;

  iovec iov[2];
  iov[0].iov_base = &size;
  iov[0].iov_len = 4;
  iov[1].iov_base = buffer;
  iov[1].iov_len = length;

  msghdr msg = {};
  msg.msg_iov = iov;
  msg.msg_iovlen = 2;
  msg.msg_name = &mcast_dest;
  msg.msg_namelen = sizeof(mcast_dest);

  sendmsg(this->m_tx_server_sock, &msg, 0);

  return 0;
}
