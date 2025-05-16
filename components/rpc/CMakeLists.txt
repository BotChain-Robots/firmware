idf_component_register(SRCS "rpc.cpp" "TCPClient.cpp"
                       PRIV_REQUIRES esp_netif
                    INCLUDE_DIRS "include")
