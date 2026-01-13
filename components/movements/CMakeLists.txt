idf_component_register(SRCS "MovementManager.cpp" "MovementManager.cpp" "TopologyManager.cpp"
                       PRIV_REQUIRES esp_event nvs_flash
                    #    REQUIRES esp_timer
                       INCLUDE_DIRS "include")
