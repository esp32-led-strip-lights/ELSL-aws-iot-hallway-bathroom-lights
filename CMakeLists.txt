# Define the minimum required version of CMake
cmake_minimum_required(VERSION 3.5)

# Include the IDF build script
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project("home-hallway-bathroom-lights")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -fdata-sections -ffunction-sections")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -fdata-sections -ffunction-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
