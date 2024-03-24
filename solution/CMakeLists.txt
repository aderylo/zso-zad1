# Minimum CMake version
cmake_minimum_required(VERSION 3.10)

# Your project name
project(YourProjectName)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# setup LIEF
set(LIEF_PATH "/home/zso/zso-zad1/solution/dependencies/LIEF")
include_directories(${LIEF_PATH}/include)
find_library(LIEF_LIBRARY NAMES LIEF PATHS ${LIEF_PATH}/lib NO_DEFAULT_PATH)

# Define your target (executable or library)
add_executable(symbolize src/main.cpp)  # or add_library for libraries

# Link your target with the LIEF library
target_link_libraries(symbolize ${LIEF_LIBRARY})
