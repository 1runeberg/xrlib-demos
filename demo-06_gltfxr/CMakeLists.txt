# xrlib demo 06 : gltfxr
# Copyright 2026 Rune Berg
# https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
# Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

set(VISIONOS OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "visionOS")
    set(VISIONOS ON)
else()
    set(CMAKE_SUPPRESS_REGENERATION true)
endif()


######################
# PROJECT DEFINITION #
######################

set(APP_NAME "gltfxr")
set(PROJECT_NAME "06_${APP_NAME}")
project("${PROJECT_NAME}" VERSION 1.0.0)

if(VISIONOS)
    add_subdirectory(visionos)
    return()
endif()

# Project directories
set(APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(APP_INCLUDE "${APP_ROOT}/src")
set(APP_SRC "${APP_ROOT}/src")
set(APP_ASSETS "${APP_ROOT}/assets/bin")

set(APP_BIN_OUT "${APP_ROOT}/bin")
set(APP_LIB_OUT "${APP_ROOT}/lib")

option(BUILD_SHADERS "Build shaders in assets directory" ON)

# Set config files
file(GLOB APP_CONFIG
        "${APP_ROOT}/.clang-format"
        "${APP_ROOT}/README.md"
        "${APP_ROOT}/LICENSE"
        "${APP_ROOT}/CMakeLists.txt"
     )

# Set headers
file(GLOB_RECURSE APP_HEADERS
        "${APP_INCLUDE}/*.h*"
        "${APP_ROOT}/visionos/*.h"
    )

# Set source code
file(GLOB_RECURSE APP_SOURCES
        "${APP_SRC}/*.c*"
    )

# Set app shaders, the PBR and night grid shaders come from xrvk
file(GLOB APP_SHADERS
        "${APP_ROOT}/assets/src/*.frag"
        "${APP_ROOT}/assets/src/*.vert"
    )


######################################
# SET PROJECT TECHNICAL REQUIREMENTS #
######################################

# C++ standard for this project
set(CPP_STD 20)
set(CMAKE_CXX_STANDARD ${CPP_STD})
set(CMAKE_CXX_STANDARD_REQUIRED True)
message(STATUS "[${APP_NAME}] Project language set to C++ ${CPP_STD}")

# Compile app shaders on the host, Android packages the bundled binaries
if(NOT ANDROID AND BUILD_SHADERS)
    find_program(GLSLC_EXECUTABLE glslc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)

    set(SHADER_OUTPUT_FILES)
    foreach(SHADER ${APP_SHADERS})
        get_filename_component(SHADER_NAME ${SHADER} NAME)
        set(OUTPUT_FILE "${APP_ASSETS}/${SHADER_NAME}.spv")

        add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND "${GLSLC_EXECUTABLE}" ${SHADER} -o ${OUTPUT_FILE}
            COMMENT "Compiling ${SHADER} to ${OUTPUT_FILE}"
            VERBATIM
            DEPENDS ${SHADER}
        )

        list(APPEND SHADER_OUTPUT_FILES ${OUTPUT_FILE})
    endforeach()

    add_custom_target(_compile_shaders_${APP_NAME} ALL DEPENDS ${SHADER_OUTPUT_FILES})
endif()

# Prepared textures are ASTC, so the app only targets Android and visionOS
if(NOT ANDROID)
    message(STATUS "[${APP_NAME}] Android and visionOS only, skipping the app for this platform")
    return()
endif()


#####################
# BINARY DEFINITION #
#####################

# Organize source folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(config FILES ${APP_CONFIG})
source_group(src FILES ${APP_HEADERS} ${APP_SOURCES})

# Get the android log library (xrlib handles the translation between its internal log class and android's)
find_library(ANDROID_LIBRARY NAMES android)
find_library(ANDROID_LOG_LIBRARY NAMES log)

# Get the android native app glue from ndk
find_path(ANDROID_NATIVE_APP_GLUE NAMES android_native_app_glue.h PATHS ${ANDROID_NDK}/sources/android/native_app_glue)

# native app glue is required for an android build, ensure android ndk is installed
if(NOT ANDROID_NATIVE_APP_GLUE)
    message(WARNING "[${APP_NAME}] WARNING: Unable to find native_app_glue in:  **${ANDROID_NDK}/sources/android/native_app_glue** setting manually.")
    set(ANDROID_NATIVE_APP_GLUE "${ANDROID_NDK}/sources/android/native_app_glue")
else()
    message(STATUS "[${APP_NAME}] Android native_app_glue found in:  ${ANDROID_NATIVE_APP_GLUE}")
endif()

add_library(android_native_app_glue OBJECT "${ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c")
target_include_directories(android_native_app_glue PUBLIC ${ANDROID_NATIVE_APP_GLUE})
target_compile_options(android_native_app_glue PRIVATE -Wno-unused-parameter)

# define library with native app glue
add_library(${APP_NAME} MODULE
            ${APP_HEADERS}
            ${APP_SOURCES}
            ${APP_CONFIG}
            $<TARGET_OBJECTS:android_native_app_glue>)
target_compile_definitions(${APP_NAME} PRIVATE XR_USE_PLATFORM_ANDROID=1)

message(STATUS "[${APP_NAME}] Project module defined.")

# Set project public include headers
target_include_directories(${APP_NAME} PUBLIC
                           ${APP_INCLUDE}
                           ${XRLIB_INCLUDE}
                           ${OPENXR_INCLUDE}
                           ${ANDROID_NATIVE_APP_GLUE}
                          )


###########################################
# LINK THIRD PARTY DEPENDENCIES TO BINARY #
###########################################

target_link_libraries(${APP_NAME}
                      ${XRLIB}
                      ${ANDROID_LIBRARY}
                      ${ANDROID_LOG_LIBRARY}
                     )

# Keep the native activity entry point when linking the glue
target_link_options(${APP_NAME} PRIVATE "-uANativeActivity_onCreate")


################
# BUILD BINARY #
################

add_dependencies(${APP_NAME} ${XRLIB})

# Set output directories
set_target_properties(${APP_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${APP_LIB_OUT}"
    LIBRARY_OUTPUT_DIRECTORY "${APP_LIB_OUT}"
    RUNTIME_OUTPUT_DIRECTORY "${APP_BIN_OUT}"
)

message(STATUS "[${APP_NAME}] Project libraries will be built in: ${APP_LIB_OUT}")
