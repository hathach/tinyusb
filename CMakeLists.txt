cmake_minimum_required(VERSION 3.28)

project(TinyUSB CXX C ASM)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOURCES "src/*.c")

add_library(TinyUSB INTERFACE)

target_include_directories(TinyUSB INTERFACE
        lib
)

target_sources(TinyUSB INTERFACE ${SOURCES})
