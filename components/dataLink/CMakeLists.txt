idf_component_register(SRCS "DataLinkManager.cpp" "DataLinkRIP.cpp" "DataLinkScheduler.cpp" "DataLinkFrames.cpp"
                       PRIV_REQUIRES driver esp_event nvs_flash esp_netif rmt
                       REQUIRES esp_timer constants
                       INCLUDE_DIRS "include")
