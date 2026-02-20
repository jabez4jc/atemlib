cmake_minimum_required(VERSION 3.20)
project(atem_bridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED BMDSWITCHER_SDK_INCLUDE_DIR)
  message(FATAL_ERROR "Set BMDSWITCHER_SDK_INCLUDE_DIR to the ATEM SDK include folder (contains BMDSwitcherAPI.h).")
endif()

add_library(atem_bridge SHARED atem_bridge.cpp)
target_include_directories(atem_bridge PRIVATE ${BMDSWITCHER_SDK_INCLUDE_DIR})

target_compile_definitions(atem_bridge PRIVATE ATEM_BRIDGE_EXPORTS)

if(APPLE)
  target_link_libraries(atem_bridge PRIVATE "-framework CoreFoundation")
endif()

if(UNIX AND NOT APPLE)
  target_link_libraries(atem_bridge PRIVATE dl pthread)
endif()
