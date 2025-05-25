#include <sys/socket.h>

#include "esp_system.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_log.h"

#define HOST_IP_ADDR "192.168.0.196"
#define PORT 3001
#define TAG "SOCKET"

static const char *payload = "Message from board";

int tcp_client() {
    char rx_buffer[128];
    char host_ip[] = HOST_IP_ADDR;
    int addr_family = AF_INET;
    int ip_protocol = IPPROTO_IP;

    struct sockaddr_in dest_addr;
    inet_pton(AF_INET, host_ip, &dest_addr.sin_addr); // Convert ipv4 address to binary
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);

    int sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        return sock;
    }

    if (0 != connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr))) {
        ESP_LOGE(TAG, "Failed to connect: %d", errno);
    }

    ESP_LOGI(TAG, "Connected");

    while(1) {
        int err = send(sock, payload, strlen(payload), 0);
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: %d", errno);
        }

        int len = recv(sock, rx_buffer, sizeof(rx_buffer) -1, 0); // blocking call
        if (len < 0) {
            ESP_LOGE(TAG, "Failed to receive: %d", errno);
        } else {
            rx_buffer[len] = 0; // temp: Null terminate to treat as a string
            ESP_LOGI(TAG, "Received %d bytes", len);
            ESP_LOGI(TAG, "%s", rx_buffer);
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}