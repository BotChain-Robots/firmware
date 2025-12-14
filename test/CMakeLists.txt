cmake_minimum_required(VERSION 3.30)
set(EXTRA_COMPONENT_DIRS "../components")

# Set the components to include the tests for.
# This can be overridden from CMake cache:
# - when invoking CMake directly: cmake -D TEST_COMPONENTS="xxxxx" ..
# - when using idf.py: idf.py -T xxxxx build
#
set(TEST_COMPONENTS "dataLink")

set(IDF_TARGET "esp32s3")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(capstone)