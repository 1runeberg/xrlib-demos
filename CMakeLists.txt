# xrlib demos
# Copyright 2024,2025 Copyright Rune Berg 
# https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
# Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
# SPDX-License-Identifier: Apache-2.0
# 
# This work is the next iteration of OpenXRProvider (v1, v2)
# OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
# OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
# v1 & v2 licensed under MIT: https://opensource.org/license/mit


cmake_minimum_required(VERSION 3.22 FATAL_ERROR)
set(CMAKE_SUPPRESS_REGENERATION true)


######################
# PROJECT DEFINITION #
######################

# Set project variables
project("xrlib_demos" VERSION 1.0.0)

# Set config files
# Set CI files
file(GLOB PROJ_CI
        "${CMAKE_CURRENT_SOURCE_DIR}/.github/workflows/*.yml"
	)
    
file(GLOB PROJ_CONFIG
        "${CMAKE_CURRENT_SOURCE_DIR}/.clang-format"
        "${CMAKE_CURRENT_SOURCE_DIR}/.gitignore"
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" 
        "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt" 
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h.in" 
	)

# Organize source folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(CI FILES ${PROJ_CI})
source_group(config FILES ${PROJ_CONFIG})

add_custom_target("_project_config" SOURCES ${PROJ_CI} ${PROJ_CONFIG})    # placeholder target to expose CI and main config files in vs project
set_target_properties("_project_config" PROPERTIES EXCLUDE_FROM_ALL TRUE)  # exclude from builds 

# Set xrlib for all demos
set(XRLIB "xrlib")

# Set project-wide options
option(BUILD_AS_STATIC "Build as static library" OFF)
option(BUILD_SHADERS "Build shaders in resource directory" ON)
option(ENABLE_XRVK "Compile xrvk - pbr render module" ON)
option(ENABLE_RENDERDOC "Enable renderdoc for render debugs" OFF) 
option(ENABLE_VULKAN_DEBUG "Enable vulkan debugging" OFF) 

# Make sure the options propagate to all subdirectories
set(BUILD_AS_STATIC ${BUILD_AS_STATIC} CACHE BOOL "Build as static library" FORCE)
set(BUILD_SHADERS ${BUILD_SHADERS} CACHE BOOL "Build shaders in resource directory" FORCE)
set(ENABLE_XRVK ${ENABLE_XRVK} CACHE BOOL "Compile xrvk - pbr render module" FORCE)
set(ENABLE_RENDERDOC ${ENABLE_RENDERDOC} CACHE BOOL "Enable renderdoc for render debugs" FORCE)
set(ENABLE_VULKAN_DEBUG ${ENABLE_VULKAN_DEBUG} CACHE BOOL "Enable vulkan debugging" FORCE)

# Add xrlib
add_subdirectory("${XRLIB}")

set(XRLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/xrlib")
set(XRLIB_INCLUDE "${XRLIB_ROOT}/include")
set(XRLIB_RES "${XRLIB_ROOT}/res")
set(XRLIB_BIN_OUT "${XRLIB_ROOT}/bin")
set(XRLIB_LIB_OUT "${XRLIB_ROOT}/lib")
set(XRLIB_THIRD_PARTY "${XRLIB_ROOT}/third_party")

set(XRVK_SHADERS_BIN "${XRLIB_ROOT}/res/shaders/bin")
set(XRVK_MODELS_BIN "${XRLIB_ROOT}/res/models/bin")

set(OPENXR_ROOT "${XRLIB_THIRD_PARTY}/openxr")
set(OPENXR_INCLUDE "${OPENXR_ROOT}/include")

# List of demos
set(DEMOS
        "demo-01_checkxr"
        "demo-02_displayxr"
        "demo-03_passthroughxr"
	    "demo-04_handtrackingxr"
        "demo-05_inputxr"
    )

# Add all demos to project
if(ANDROID)
    # Only build current project in Android, defined in build.gradle (cmakeArgs)
    add_subdirectory("${ANDROID_PROJECT}")
    message("Android project to build: ${ANDROID_PROJECT}")
else()
    foreach(DEMO ${DEMOS})
        add_subdirectory("${DEMO}")
    endforeach()
endif()

# Set startup app
if(MSVC)
    set_property (DIRECTORY PROPERTY VS_STARTUP_PROJECT "inputxr")
    message("Startup project for VS set to: inputxr")
endif()
