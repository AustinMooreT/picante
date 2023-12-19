cmake_minimum_required(VERSION 3.27)
project(picante VERSION 0.0.0 LANGUAGES CXX)

find_package(Vulkan REQUIRED)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(picante main.cpp)

target_link_libraries(picante Vulkan::Vulkan)
