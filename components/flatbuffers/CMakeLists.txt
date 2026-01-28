idf_component_register(SRCS "MPIMessageBuilder.cpp" 
    "AngleControlMessageBuilder.cpp" 
    "TopologyMessageBuilder.cpp" 
    "SensorMessageBuilder.cpp" 
    "TopologyBuilder.cpp"
    "MovementSetBuilder.cpp"
    REQUIRES "constants"
    INCLUDE_DIRS "include"
)
