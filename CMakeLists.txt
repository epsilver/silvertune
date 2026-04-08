cmake_minimum_required(VERSION 3.21)
project(silvertune VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(AUBIO REQUIRED aubio)

# clap-wrapper (provides make_clapfirst_plugins, clap target, etc.)
set(CLAP_SDK_ROOT ${CMAKE_SOURCE_DIR}/clap)
set(CLAP_WRAPPER_DOWNLOAD_DEPENDENCIES TRUE)
add_subdirectory(libs/clap-wrapper)

# Static library with all the DSP and plugin logic
add_library(silvertune-impl STATIC
    src/plugin.cpp
    src/processor.cpp
    src/parameters.cpp
    src/scale.cpp
)

target_include_directories(silvertune-impl PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/clap/include
    ${AUBIO_INCLUDE_DIRS}
)

target_link_libraries(silvertune-impl PUBLIC
    clap
    ${AUBIO_LIBRARIES}
)

target_link_directories(silvertune-impl PUBLIC
    ${AUBIO_LIBRARY_DIRS}
)

# Build CLAP + VST3 from the static library
make_clapfirst_plugins(
    TARGET_NAME silvertune
    IMPL_TARGET silvertune-impl

    OUTPUT_NAME "Silvertune"

    ENTRY_SOURCE "src/entry.cpp"

    BUNDLE_IDENTIFIER "com.silvertune.silvertune"
    BUNDLE_VERSION ${PROJECT_VERSION}

    COPY_AFTER_BUILD FALSE

    PLUGIN_FORMATS CLAP VST3
)
