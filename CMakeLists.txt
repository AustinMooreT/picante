cmake_minimum_required(VERSION 3.27)
project(picante VERSION 0.0.0 LANGUAGES CXX)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(glfw3 REQUIRED)

find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

function(compile_shader target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "ENV;FORMAT" "SOURCES")
    foreach(source ${arg_SOURCES})
        add_custom_command(
            OUTPUT ${source}.bin
            DEPENDS ${source}
            COMMAND
                ${glslc_executable}
               	-o ${source}.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/${source}
        )
        target_sources(${target} PRIVATE ${source}.bin)
    endforeach()
endfunction()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(picante main.cpp)
compile_shader(picante
  SOURCES
    picante.vert
    picante.frag)

target_link_libraries(picante Vulkan::Vulkan glfw)
