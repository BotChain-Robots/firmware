idf_component_register(SRCS "DataLinkManager.cpp"
                       PRIV_REQUIRES driver esp_event nvs_flash esp_netif rmt
                       INCLUDE_DIRS "include")
