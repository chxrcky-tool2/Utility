cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(ChxrCkyBot VERSION 1.0.6)

file(GLOB SOURCES src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Third-party libraries that were written for Geode 5 and may not build against Geode 6 alpha yet.
# If CI fails inside one of them, turn that option OFF (the source files already compile to stubs
# without the library: no GDR import / no ImGui menu, the Cocos menu stays).
option(CKY_GDR   "Import .gdr replays (libGDR)"        ON)
option(CKY_IMGUI "ImGui menu (gd-imgui-cocos)"          ON)

if (CKY_GDR)
    CPMAddPackage("gh:maxnut/GDReplayFormat#gdr2")
    target_link_libraries(${PROJECT_NAME} libGDR)
endif()

if (CKY_IMGUI)
    CPMAddPackage("gh:matcool/gd-imgui-cocos#geode")
    target_link_libraries(${PROJECT_NAME} imgui-cocos)
endif()

# Zstd (CKZ5 macro compression)
CPMAddPackage(
    NAME zstd
    GITHUB_REPOSITORY facebook/zstd
    GIT_TAG v1.5.6
    SOURCE_SUBDIR build/cmake
    OPTIONS "ZSTD_BUILD_PROGRAMS OFF" "ZSTD_BUILD_TESTS OFF" "ZSTD_BUILD_SHARED OFF" "ZSTD_BUILD_STATIC ON" "ZSTD_LEGACY_SUPPORT OFF"
)
target_link_libraries(${PROJECT_NAME} libzstd_static)
target_include_directories(${PROJECT_NAME} PRIVATE ${zstd_SOURCE_DIR}/lib)

# Built-in renderer: Android hardware encoder + MP4 muxer from the NDK (no other mod needed)
if (ANDROID)
    target_link_libraries(${PROJECT_NAME} mediandk)
endif()

setup_geode_mod(${PROJECT_NAME})
