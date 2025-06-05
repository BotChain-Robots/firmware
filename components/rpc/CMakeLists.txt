idf_component_register(SRCS "rpc.cpp" "WifiManager.cpp" "mDNSDiscoveryService.cpp" "TCPServer"
                       PRIV_REQUIRES driver esp_event nvs_flash esp_netif esp_wifi espressif__mdns constants
                       INCLUDE_DIRS "include")
