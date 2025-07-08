idf_component_register(SRCS "WifiManager.cpp" "mDNSDiscoveryService.cpp" "TCPServer.cpp" "CommunicationRouter.cpp" "MessagingInterface.cpp"
                       PRIV_REQUIRES driver esp_event nvs_flash esp_netif esp_wifi espressif__mdns constants config flatbuffers
                       INCLUDE_DIRS "include")
