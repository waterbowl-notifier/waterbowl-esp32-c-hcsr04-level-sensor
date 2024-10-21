# Define the minimum required version of CMake
cmake_minimum_required(VERSION 3.5)

# Include the IDF build script
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project name
project(waterbowl-sensor)

set(EXTRA_COMPONENT_DIRS ${CMAKE_SOURCE_DIR}/main)
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")

