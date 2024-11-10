set(CMAKE_TOOLCHAIN_FILE "vcpkg 설치 경로" CACHE STRING "vcpkg toolchain file")

cmake_minimum_required(VERSION 3.28)
project(Visualizer C)

set(CMAKE_C_STANDARD 11)

add_executable(Visualizer main.c)

find_package(CURL REQUIRED)
target_link_libraries(Visualizer PRIVATE CURL::libcurl)

find_package(json-c CONFIG REQUIRED)
target_link_libraries(Visualizer PRIVATE json-c::json-c)
