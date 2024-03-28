cmake_minimum_required(VERSION 3.16)
project(YourProjectName VERSION 1.0 LANGUAGES CXX)

add_subdirectory(deps/ELFIO)

add_executable(symbolize src/main.cpp)
target_include_directories(symbolize PRIVATE src)
target_link_libraries(symbolize PRIVATE elfio::elfio)

file(GLOB SRC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")

# Precompile the most commonly used ELFIO headers
target_precompile_headers(symbolize PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/deps/ELFIO/elfio/elfio.hpp>
    ${SRC_HEADERS}
)

set_target_properties(symbolize PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
