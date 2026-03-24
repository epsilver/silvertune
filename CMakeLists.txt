cmake_minimum_required(VERSION 3.16)
project(silvertune VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(AUBIO REQUIRED aubio)

# Plugin shared library
add_library(silvertune MODULE
    src/plugin.cpp
    src/processor.cpp
    src/parameters.cpp
    src/scale.cpp
)

target_include_directories(silvertune PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/clap/include
    ${AUBIO_INCLUDE_DIRS}
)

target_link_libraries(silvertune PRIVATE
    ${AUBIO_LIBRARIES}
)

target_link_directories(silvertune PRIVATE
    ${AUBIO_LIBRARY_DIRS}
)

# CLAP plugins use .clap extension
set_target_properties(silvertune PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Install target
install(FILES "${CMAKE_BINARY_DIR}/silvertune.clap"
    DESTINATION "$ENV{HOME}/.clap"
)
