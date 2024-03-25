cmake_minimum_required(VERSION 3.10)
project(YourProjectName VERSION 1.0 LANGUAGES CXX)

add_subdirectory(deps/ELFIO)

add_executable(symbolize src/main.cpp)
target_include_directories(symbolize PRIVATE src)
target_link_libraries(symbolize PRIVATE elfio::elfio)

set_target_properties(symbolize PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
