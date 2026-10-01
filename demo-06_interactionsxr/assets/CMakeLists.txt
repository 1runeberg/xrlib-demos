# interactionsxr asset preprocessing (host tools only)
# Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.28)
project(interactionsxr_assets LANGUAGES C CXX)

# Environment baking is slow without optimisation
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Asset tool build type" FORCE)
endif()

set(XRLIB_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../xrlib" CACHE PATH "xrlib checkout")
add_subdirectory("${XRLIB_SOURCE}/tools/xrvk" "${CMAKE_CURRENT_BINARY_DIR}/xrvk-tools")

# Prepared night grid lighting and sky are bundled, regeneration uses the xrvk CPU baker
# Keep --floor-size in sync with SBackdrop::floorSize in src/backdrop.hpp
add_custom_target(regenerate_environment
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/bin/PreparedNightGrid"
    COMMAND xrvk-tools night-grid
        --ibl "${CMAKE_CURRENT_SOURCE_DIR}/bin/PreparedNightGrid/night_grid.ibl"
        --sky "${CMAKE_CURRENT_SOURCE_DIR}/bin/PreparedNightGrid/night_grid.sky"
        --width 2048 --floor-size 4.5 --diffuse-size 32 --specular-size 256 --lut-size 128 --samples 1024
        --star-reflections 10
    VERBATIM USES_TERMINAL)

add_dependencies(regenerate_environment xrvk-tools)
