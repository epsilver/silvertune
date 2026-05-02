cmake_minimum_required(VERSION 3.21)
project(silvertune VERSION 0.1.1 LANGUAGES CXX)

# MSVC requires C++20 for designated initializers used in CLAP headers
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# AUv2 uses Objective-C++ — must be enabled before clap-wrapper configures
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

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
    src/yin.cpp
)

if(WIN32)
    target_sources(silvertune-impl PRIVATE src/gui_win32.cpp)
elseif(APPLE)
    target_sources(silvertune-impl PRIVATE src/gui_cocoa.mm)
    target_link_libraries(silvertune-impl PUBLIC
        "-framework Cocoa"
        "-framework CoreGraphics"
    )
else()
    target_sources(silvertune-impl PRIVATE src/gui_x11.cpp)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(XFT REQUIRED xft)
    find_package(X11 REQUIRED)
    target_link_libraries(silvertune-impl PUBLIC ${X11_LIBRARIES} ${XFT_LIBRARIES})
    target_include_directories(silvertune-impl PUBLIC ${X11_INCLUDE_DIR} ${XFT_INCLUDE_DIRS})
    target_link_libraries(silvertune-impl PUBLIC pthread)
endif()

target_include_directories(silvertune-impl PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/clap/include
)

target_link_libraries(silvertune-impl PUBLIC
    clap
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

    PLUGIN_FORMATS CLAP VST3 AUV2

    AUV2_MANUFACTURER_NAME "Vertical Rectangle"
    AUV2_MANUFACTURER_CODE "VtRc"
    AUV2_SUBTYPE_CODE "Tune"
    AUV2_INSTRUMENT_TYPE "aufx"
)
