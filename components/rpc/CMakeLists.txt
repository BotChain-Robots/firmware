idf_component_register(SRCS "rpc.cpp" "WifiManager.cpp"
                       PRIV_REQUIRES driver esp_event nvs_flash esp_netif esp_wifi
                       INCLUDE_DIRS "include")
