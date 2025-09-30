idf_component_register(SRCS "WifiManager.cpp" "mDNSDiscoveryService.cpp" "TCPServer.cpp" "CommunicationRouter.cpp" "MessagingInterface.cpp" "OrientationDetection.cpp"
                        PRIV_REQUIRES driver esp_event nvs_flash esp_netif espressif__mdns constants config flatbuffers dataLink rmt
                        REQUIRES ptrQueue esp_wifi
                        INCLUDE_DIRS "include")

target_compile_options(${COMPONENT_LIB} PRIVATE -fexceptions)
