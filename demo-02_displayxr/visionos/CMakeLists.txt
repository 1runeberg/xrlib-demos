# xrlib demo 02 : displayxr - visionOS
# Copyright 2026 Rune Berg
# SPDX-License-Identifier: Apache-2.0

if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
    message(FATAL_ERROR "Use the Xcode generator for visionOS: -G Xcode")
endif()

enable_language(Swift)
get_filename_component(APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(APPLE_TEAM "" CACHE STRING "Apple development team")

################
# DEPENDENCIES #
################

if(NOT TARGET xrlib)
    set(XRLIB_SOURCE "${APP_ROOT}/../xrlib" CACHE PATH "xrlib source repo")
    include("${APP_ROOT}/../cmake/visionos.cmake")
    add_subdirectory("${XRLIB_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/xrlib")
endif()

get_target_property(XRLIB_SOURCE xrlib SOURCE_DIR)
get_target_property(OPENXR_LIBRARY VisionOpenXR::Runtime IMPORTED_LOCATION)
get_target_property(MOLTENVK_LIBRARY VisionOpenXR::MoltenVK IMPORTED_LOCATION)
get_filename_component(OPENXR_FRAMEWORK "${OPENXR_LIBRARY}" DIRECTORY)
get_filename_component(MOLTENVK_FRAMEWORK "${MOLTENVK_LIBRARY}" DIRECTORY)
get_filename_component(SDK_DIRECTORY "${OPENXR_FRAMEWORK}" DIRECTORY)

###########
# SHADERS #
###########

find_program(GLSLC_EXECUTABLE glslc HINTS "$ENV{VULKAN_SDK}/bin" REQUIRED)
file(GLOB APP_SHADER_INCLUDES CONFIGURE_DEPENDS "${XRLIB_SOURCE}/res/shaders/src/*.glsl")
set(APP_SHADERS)
foreach(SHADER primitive.vert primitive.frag stencil0.vert stencil1.vert stencil.frag)
    set(SHADER_SOURCE "${XRLIB_SOURCE}/res/shaders/src/${SHADER}")
    set(SHADER_BINARY "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER}.spv")
    add_custom_command(OUTPUT "${SHADER_BINARY}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.2 "${SHADER_SOURCE}" -o "${SHADER_BINARY}"
        DEPENDS "${SHADER_SOURCE}" ${APP_SHADER_INCLUDES} "${GLSLC_EXECUTABLE}"
        VERBATIM)
    list(APPEND APP_SHADERS "${SHADER_BINARY}")
endforeach()
set_source_files_properties(${APP_SHADERS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

#####################
# BINARY DEFINITION #
#####################

add_executable(${APP_NAME} MACOSX_BUNDLE
    "${APP_ROOT}/src/main.cpp"
    app.swift
    visionos_app_bridge.h
    "${SDK_DIRECTORY}/Support/immersion_host.swift"
    ${APP_SHADERS})
target_compile_features(${APP_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${APP_NAME} PRIVATE XRVK_ENABLED XR_USE_PLATFORM_VISIONOS)
target_link_libraries(${APP_NAME} PRIVATE xrlib VisionOpenXR::Runtime "-framework Metal" "-framework Foundation")

set_target_properties(${APP_NAME} PROPERTIES
    OUTPUT_NAME DisplayXR
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    XCODE_ATTRIBUTE_SWIFT_VERSION 5.0
    MACOSX_BUNDLE_GUI_IDENTIFIER io.runeberg.xrlib.displayxr
    MACOSX_BUNDLE_BUNDLE_VERSION 1
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    XCODE_GENERATE_SCHEME TRUE
    XCODE_EMBED_FRAMEWORKS "${OPENXR_FRAMEWORK};${MOLTENVK_FRAMEWORK}"
    XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY TRUE
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER io.runeberg.xrlib.displayxr
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/visionos_app_bridge.h"
    XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES YES
    XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE YES
    XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleDisplayName displayxr
    XCODE_ATTRIBUTE_INFOPLIST_KEY_NSWorldSensingUsageDescription "OpenXR uses world tracking to place the scene in your room"
    XCODE_ATTRIBUTE_INFOPLIST_KEY_UIApplicationSceneManifest_Generation YES
    XCODE_ATTRIBUTE_INFOPLIST_KEY_UILaunchScreen_Generation YES
    XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "$(inherited) @executable_path/Frameworks")

if(APPLE_TEAM)
    set_target_properties(${APP_NAME} PROPERTIES
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${APPLE_TEAM}"
        XCODE_ATTRIBUTE_CODE_SIGN_STYLE Automatic
        XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY TRUE)
else()
    set_target_properties(${APP_NAME} PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
endif()
