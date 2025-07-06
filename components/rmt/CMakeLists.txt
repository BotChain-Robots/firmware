idf_component_register(SRCS "RMTManager.cpp"
                       PRIV_REQUIRES driver esp_event nvs_flash esp_netif
                       REQUIRES esp_driver_rmt
                       INCLUDE_DIRS "include")
