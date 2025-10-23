file(GLOB_RECURSE ALL_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.S"
)

idf_component_register(SRCS ${ALL_SRCS}
                        PRIV_REQUIRES driver esp_event nvs_flash esp_netif espressif__mdns constants config flatbuffers dataLink rmt
                        REQUIRES ptrQueue esp_wifi
                        INCLUDE_DIRS "include")

target_compile_options(${COMPONENT_LIB} PRIVATE -fexceptions)
