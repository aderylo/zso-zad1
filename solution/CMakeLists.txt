cmake_minimum_required(VERSION 3.10)
project(YourProjectName VERSION 1.0 LANGUAGES CXX)

# Now pointing to the ELFIO directory within the deps directory
add_subdirectory(deps/ELFIO)

# Replace "your_source_file.cpp" with the actual source files of your project
add_executable(symbolize src/main.cpp)
target_link_libraries(symbolize PRIVATE elfio::elfio)

# If your project requires C++17
set_target_properties(symbolize PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

