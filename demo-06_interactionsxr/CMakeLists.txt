# xrlib demo 06 : interactionsxr
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

set(APP_NAME "interactionsxr")
set(PROJECT_NAME "05_${APP_NAME}")
project("${PROJECT_NAME}" VERSION 1.0.0)

# Project directories
set(APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(APP_INCLUDE "${APP_ROOT}/src")
set(APP_SRC "${APP_ROOT}/src")
set(APP_ASSETS "${APP_ROOT}/assets/bin")
set(XRAPP "../${CMAKE_CURRENT_PROJECT_DIR}/xrapp")

set(APP_BIN_OUT "${APP_ROOT}/bin")
set(APP_LIB_OUT "${APP_ROOT}/lib")

option(BUILD_SHADERS "Build shaders in assets directory" ON)

# Set config files
file(GLOB APP_CONFIG
        "${APP_ROOT}/.clang-format"
        "${APP_ROOT}/.gitignore"
        "${APP_ROOT}/README.md"
        "${APP_ROOT}/LICENSE" 
        "${APP_ROOT}/CMakeLists.txt" 
        "${APP_ROOT}/CMakeLists.txt" 
     )
       
# Set CI files
file(GLOB APP_CI
        "${APP_ROOT}/.github/workflows/*.yml"
	)

# Set headers
file(GLOB_RECURSE APP_HEADERS
        "${APP_INCLUDE}/*.h*"
        "${APP_SRC}/*.h*"
        "${XRAPP}/*.h*"
    )

# Set source code
file(GLOB_RECURSE APP_SOURCES
        "${APP_SRC}/*.c*"
        "${XRAPP}/*.c*"
    )


######################################
# SET PROJECT TECHNICAL REQUIREMENTS #
######################################

# C++ standard for this project
set(CPP_STD 20)
set(CMAKE_CXX_STANDARD ${CPP_STD})
set(CMAKE_CXX_STANDARD_REQUIRED True)
message(STATUS "[${APP_NAME}] Project language set to C++ ${CPP_STD}")

# Check platform architecture
if(NOT PLATFORM)
	if(CMAKE_SIZEOF_VOID_P MATCHES 8)
	    set(PLATFORM 64)
	else()
        message(FATAL_ERROR "[${APP_NAME}] ERROR: Only 64-bit platforms are supported.")
	endif()
endif()

# Debug settings
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Assertions enabled
    message(STATUS "[${APP_NAME}] Debug build: Assertions enabled.")
else()
    # Disable assertions by defining NDEBUG
    add_definitions(-DNDEBUG)
    message(STATUS "[${APP_NAME}] Release build: Assertions disabled.")
endif()


#####################
# BINARY DEFINITION #
#####################

# Organize source folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(config FILES ${APP_CONFIG})
source_group(src FILES ${APP_HEADERS} ${APP_SOURCES})


# Define application executable/module
if(ANDROID)
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

    add_library( android_native_app_glue OBJECT "${ANDROID_NATIVE_APP_GLUE}/android_native_app_glue.c")
    target_include_directories( android_native_app_glue PUBLIC ${ANDROID_NATIVE_APP_GLUE})
    target_compile_options( android_native_app_glue PRIVATE -Wno-unused-parameter)

    # define library with native app glue
    add_library(${APP_NAME} MODULE 
                ${APP_HEADERS}
                ${APP_SOURCES}
                ${APP_CONFIG}
                $<TARGET_OBJECTS:android_native_app_glue>)
    target_compile_definitions(${APP_NAME} PRIVATE XR_USE_PLATFORM_ANDROID=1)   

else()
    # for desktop builds, we only need to define the executable itself
    add_executable(${APP_NAME} 
                   ${APP_HEADERS} 
                   ${APP_SOURCES}
                   ${APP_CONFIG}
                   ${APP_CI}
                )
endif()

message(STATUS "[${APP_NAME}] Project executable/module defined.")

# Set project public include headers
target_include_directories(${APP_NAME} PUBLIC 
                           ${APP_INCLUDE}
                           ${APP_SRC}
                           ${XRAPP}
                           ${XRLIB_INCLUDE}
                           ${OPENXR_INCLUDE}
                          )

if(ANDROID)
    # For android projects, add native app glue includes
    target_include_directories(${APP_NAME} PUBLIC ${ANDROID_NATIVE_APP_GLUE})
endif()

message(STATUS "[${APP_NAME}] Public include directories defined.")


###########################################
# LINK THIRD PARTY DEPENDENCIES TO BINARY #
########################################### 

# All platforms libraries
target_link_libraries(${APP_NAME}  
                      ${XRLIB}
                     )

if(ANDROID)
    # Android only libraries
    target_link_libraries(${APP_NAME} 
                          ${ANDROID_LIBRARY} 
                          ${ANDROID_LOG_LIBRARY}
                         )
endif()

message(STATUS "[${APP_NAME}] Third party libraries linked.")


################
# BUILD BINARY #
################

add_dependencies(${APP_NAME} ${XRLIB})

# Set output directories
set_target_properties(${APP_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${APP_LIB_OUT}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${APP_LIB_OUT}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${APP_BIN_OUT}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${APP_LIB_OUT}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${APP_LIB_OUT}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${APP_BIN_OUT}"

    VS_DEBUGGER_WORKING_DIRECTORY "${APP_BIN_OUT}"
)

message(STATUS "[${APP_NAME}] Current source directory is: ${APP_SRC}")
message(STATUS "[${APP_NAME}] Current build/working directory is: ${CMAKE_CURRENT_BINARY_DIR}")

message(STATUS "[${APP_NAME}] Project archives will be built in: ${APP_LIB_OUT}")
message(STATUS "[${APP_NAME}] Project libraries will be built in: ${APP_LIB_OUT}")
message(STATUS "[${APP_NAME}] Project binaries will be built in: ${APP_BIN_OUT}")

message(STATUS "[${APP_NAME}] xrlib binaries are in: ${XRLIB_BIN_OUT}")

# Compile shaders
file(GLOB_RECURSE APP_SHADERS
        "${APP_ASSETS}/../src/*.frag"
        "${APP_ASSETS}/../src/*.vert"
        "${APP_ASSETS}/../src/*.tesc"
        "${APP_ASSETS}/../src/*.tese"
        "${APP_ASSETS}/../src/*.geom"
        "${APP_ASSETS}/../src/*.comp"
	)

if(NOT ANDROID AND BUILD_SHADERS)
    set(SHADER_OUTPUT_FILES)
    foreach(SHADER ${APP_SHADERS})
        # Get the filename with extension
        get_filename_component(SHADER_NAME ${SHADER} NAME)

        # Define the output .spv file path
        set(OUTPUT_FILE "${APP_ASSETS}/${SHADER_NAME}.spv") 

        # Add custom command to run glslc for each shader file
        add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND glslc ${SHADER} -o ${OUTPUT_FILE}
            COMMENT "Compiling ${SHADER} to ${OUTPUT_FILE}"
            VERBATIM
            DEPENDS ${SHADER}  # Check if the shader file has changed
        )

        # Collect all output files
        list(APPEND SHADER_OUTPUT_FILES ${OUTPUT_FILE})
    endforeach()

    # Create a custom target that depends on all shader output files
    add_custom_target(_compile_shaders_${APP_NAME} ALL DEPENDS ${SHADER_OUTPUT_FILES})  
endif()

# Post-Build
add_custom_command( 
                    TARGET ${APP_NAME} POST_BUILD

                    # create out directories
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${APP_BIN_OUT}"

                    # Copy app assets to out binary directory
                    #COMMAND ${CMAKE_COMMAND} -E copy_directory "${APP_ASSETS}" "${APP_BIN_OUT}"

                    # copy xrlib to out binary directory
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "${XRLIB_BIN_OUT}" "${APP_BIN_OUT}"
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "${XRLIB_LIB_OUT}" "${APP_BIN_OUT}"

                    # copy xrvk resources to out binary directory
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "${XRVK_SHADERS_BIN}" "${APP_BIN_OUT}"
                    COMMAND ${CMAKE_COMMAND} -E copy_directory "${XRVK_MODELS_BIN}" "${APP_BIN_OUT}"
                  )

# Compile shaders only if we're not in an android build
if(NOT ANDROID AND BUILD_SHADERS)
add_custom_command(TARGET ${APP_NAME} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} --build . --target _compile_shaders_${APP_NAME} 
                  )
endif()