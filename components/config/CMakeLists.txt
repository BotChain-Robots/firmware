idf_component_register(SRCS "ConfigManager.cpp"
                        PRIV_REQUIRES constants nvs_flash
                        REQUIRES flatbuffers
                        INCLUDE_DIRS "include")
