# xrlib demos - visionOS dependencies
# Copyright 2026 Rune Berg
# SPDX-License-Identifier: Apache-2.0

set(VISION_OPENXR_SDK "" CACHE PATH "Public vision-openxr-sdk repo")

if(NOT EXISTS "${VISION_OPENXR_SDK}/CMakeLists.txt")
    message(FATAL_ERROR "Set VISION_OPENXR_SDK to the public SDK repo")
endif()

set(VISION_OPENXR_BACKEND vulkan)
add_subdirectory("${VISION_OPENXR_SDK}" "${CMAKE_CURRENT_BINARY_DIR}/sdk")
get_target_property(XRLIB_OPENXR_LIBRARY VisionOpenXR::Runtime IMPORTED_LOCATION)
get_target_property(Vulkan_LIBRARY VisionOpenXR::MoltenVK IMPORTED_LOCATION)
get_target_property(Vulkan_INCLUDE_DIR VisionOpenXR::MoltenVK INTERFACE_INCLUDE_DIRECTORIES)

set(BUILD_AS_STATIC ON)
set(BUILD_SHADERS OFF)
set(ENABLE_XRVK ON)
set(ENABLE_RENDERDOC OFF)
set(ENABLE_VULKAN_DEBUG OFF)
set(XRLIB_LIB_OUT "${CMAKE_BINARY_DIR}/lib/$<CONFIG>" CACHE PATH "Library output directory")
set(XRLIB_BIN_OUT "${CMAKE_BINARY_DIR}/bin/$<CONFIG>" CACHE PATH "Binary output directory")
