idf_component_register(SRCS "MovementManager.cpp" "MovementSetManager.cpp" "TopologyManager.cpp"
                       PRIV_REQUIRES esp_event nvs_flash rmt dataLink flatbuffers
                    #    REQUIRES esp_timer
                       INCLUDE_DIRS "include")
