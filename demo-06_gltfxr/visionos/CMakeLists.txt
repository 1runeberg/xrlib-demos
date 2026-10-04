# xrlib demo 06 : gltfxr - visionOS
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
foreach(SHADER mesh_pbr_skin.vert mesh_pbr.frag backdrop.vert backdrop_far.vert night_grid_sky.frag night_grid_floor.frag night_grid_stars.vert night_grid_stars.frag log.vert log.frag)
    set(SHADER_SOURCE "${XRLIB_SOURCE}/res/shaders/src/${SHADER}")
    if(SHADER MATCHES "^log\\.")
        set(SHADER_SOURCE "${APP_ROOT}/assets/src/${SHADER}")
    endif()

    set(SHADER_BINARY "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER}.spv")
    add_custom_command(OUTPUT "${SHADER_BINARY}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.2 -I "${XRLIB_SOURCE}/res/shaders/src" "${SHADER_SOURCE}" -o "${SHADER_BINARY}"
        DEPENDS "${SHADER_SOURCE}" ${APP_SHADER_INCLUDES} "${GLSLC_EXECUTABLE}"
        VERBATIM)
    list(APPEND APP_SHADERS "${SHADER_BINARY}")
endforeach()
set_source_files_properties(${APP_SHADERS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

##########
# ASSETS #
##########

# Bundle the prepared models and lighting with their folder layout
file(GLOB_RECURSE APP_ASSETS CONFIGURE_DEPENDS "${APP_ROOT}/assets/bin/Prepared*/*")
list(FILTER APP_ASSETS EXCLUDE REGEX "/\\.DS_Store$")
foreach(ASSET ${APP_ASSETS})
    file(RELATIVE_PATH ASSET_PATH "${APP_ROOT}/assets/bin" "${ASSET}")
    get_filename_component(ASSET_DIRECTORY "${ASSET_PATH}" DIRECTORY)
    set_source_files_properties("${ASSET}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${ASSET_DIRECTORY}")
endforeach()

set_source_files_properties(Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

#####################
# BINARY DEFINITION #
#####################

add_executable(${APP_NAME} MACOSX_BUNDLE
    "${APP_ROOT}/src/main.cpp"
    app.swift
    visionos_app_bridge.h
    Assets.xcassets
    "${SDK_DIRECTORY}/Support/application_host.swift"
    "${SDK_DIRECTORY}/Support/immersion_host.swift"
    "${SDK_DIRECTORY}/Support/selection_input.swift"
    ${APP_SHADERS}
    ${APP_ASSETS})
target_include_directories(${APP_NAME} PRIVATE "${APP_ROOT}/src")
target_compile_features(${APP_NAME} PRIVATE cxx_std_20)
target_compile_definitions(${APP_NAME} PRIVATE XRVK_ENABLED XR_USE_PLATFORM_VISIONOS)
target_link_libraries(${APP_NAME} PRIVATE xrlib VisionOpenXR::Runtime "-framework Metal" "-framework Foundation")

# Launch into the mixed immersive space using the plist scene manifest
set_target_properties(${APP_NAME} PROPERTIES
    OUTPUT_NAME GltfXR
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    XCODE_ATTRIBUTE_SWIFT_VERSION 5.0
    MACOSX_BUNDLE_GUI_IDENTIFIER io.runeberg.xrlib.gltfxr
    MACOSX_BUNDLE_BUNDLE_VERSION 1
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    XCODE_GENERATE_SCHEME TRUE
    XCODE_EMBED_FRAMEWORKS "${OPENXR_FRAMEWORK};${MOLTENVK_FRAMEWORK}"
    XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY TRUE
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER io.runeberg.xrlib.gltfxr
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/visionos_app_bridge.h"
    XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES YES
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME AppIcon
    XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE YES
    XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleDisplayName gltfxr
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
