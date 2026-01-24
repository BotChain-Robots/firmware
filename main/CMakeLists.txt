file(GLOB_RECURSE ALL_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.S"
)

if(DEFINED RMT_TEST AND RMT_TEST EQUAL 1)
    message(STATUS "Building for testing RMT/Link Layer Functionality with main_rmt_test.cpp")
    list(REMOVE_ITEM ALL_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/main_preprogrammed_test.cpp"
    )
    idf_build_set_property(COMPILE_DEFINITIONS RMT_TEST APPEND)
elseif(DEFINED PP_MOVE AND PP_MOVE EQUAL 1)
    message(STATUS "Building for testing preprogrammed movements")
    list(REMOVE_ITEM ALL_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/main_rmt_test.cpp"
    )
    idf_build_set_property(COMPILE_DEFINITIONS PP_MOVE APPEND)
else()
    list(REMOVE_ITEM ALL_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/main_rmt_test.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/main_preprogrammed_test.cpp"
    )
endif()

idf_component_register(SRCS ${ALL_SRCS}
                       PRIV_REQUIRES esp_psram spi_flash nvs_flash esp_event rpc constants config rmt esp_driver_gptimer dataLink flatbuffers esp_driver_ledc movements
                       INCLUDE_DIRS "include")

if(DEFINED SRC_BOARD AND SRC_BOARD)
    message(STATUS "Building for source board id: " ${SRC_BOARD})
    idf_build_set_property(COMPILE_DEFINITIONS SRC_BOARD=${SRC_BOARD} APPEND)
endif()

if(DEFINED RECEIVER_BOARD AND RECEIVER_BOARD)
    message(STATUS "Building for receiver board id: " ${RECEIVER_BOARD})
    idf_build_set_property(COMPILE_DEFINITIONS RECEIVER_BOARD=${RECEIVER_BOARD} APPEND)
endif()

if(DEFINED RECEIVE_ONLY AND RECEIVE_ONLY)
    message(STATUS "Building for receive only")
    idf_build_set_property(COMPILE_DEFINITIONS RECEIVE_ONLY=${RECEIVE_ONLY} APPEND)
endif()
