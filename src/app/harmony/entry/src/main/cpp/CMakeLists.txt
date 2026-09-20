cmake_minimum_required(VERSION 3.20)
project(HRKEntry LANGUAGES CXX)
set(HRK_HARMONY ON CACHE BOOL "Harmony platform" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../.." "${CMAKE_CURRENT_BINARY_DIR}/hrk")
