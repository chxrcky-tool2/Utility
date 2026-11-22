cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(ChxrCkyBot VERSION 1.0.6)

option(CKY_GDR   "Import .gdr replays (libGDR)"   ON)
option(CKY_IMGUI "ImGui menu (gd-imgui-cocos)"    ON)

file(GLOB SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

if (CKY_GDR)
    CPMAddPackage("gh:maxnut/GDReplayFormat#gdr2")
    target_link_libraries(${PROJECT_NAME} libGDR)
endif()

if (CKY_IMGUI)
    CPMAddPackage(
        NAME gd-imgui-cocos
        GITHUB_REPOSITORY matcool/gd-imgui-cocos
        GIT_TAG geode
    )
    message(STATUS "imgui-cocos (matcool/gd-imgui-cocos, geode) source: ${gd-imgui-cocos_SOURCE_DIR}")
    target_link_libraries(${PROJECT_NAME} imgui-cocos)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CKY_IMGUI_ENABLED=1)
endif()

CPMAddPackage(
    NAME zstd
    GITHUB_REPOSITORY facebook/zstd
    GIT_TAG v1.5.6
    SOURCE_SUBDIR build/cmake
    OPTIONS "ZSTD_BUILD_PROGRAMS OFF" "ZSTD_BUILD_TESTS OFF" "ZSTD_BUILD_SHARED OFF" "ZSTD_BUILD_STATIC ON" "ZSTD_LEGACY_SUPPORT OFF"
)
target_link_libraries(${PROJECT_NAME} libzstd_static)
target_include_directories(${PROJECT_NAME} PRIVATE ${zstd_SOURCE_DIR}/lib)

if (ANDROID)
    target_link_libraries(${PROJECT_NAME} mediandk)
endif()

setup_geode_mod(${PROJECT_NAME})
