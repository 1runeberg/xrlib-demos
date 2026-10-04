# gltfxr asset preprocessing (host tools only)
# Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.28)
project(gltfxr_assets LANGUAGES C CXX)

# Environment baking is slow without optimisation
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Asset tool build type" FORCE)
endif()

set(XRLIB_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../xrlib" CACHE PATH "xrlib checkout")
add_subdirectory("${XRLIB_SOURCE}/tools/xrvk" "${CMAKE_CURRENT_BINARY_DIR}/xrvk-tools")

find_program(KTX_EXECUTABLE NAMES ktx DOC "KTX-Software host executable")

# Prepared models are bundled, gltfxr only runs on mobile GPUs so textures are ASTC
add_custom_target(preprocess_assets ALL)
add_custom_target(regenerate_assets)

foreach(MODEL DamagedHelmet Fox BrainStem FlightHelmet Saber SteamVRGlove)
    set(OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/Prepared${MODEL}")
    if(MODEL STREQUAL "DamagedHelmet")
        set(OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/PreparedHelmet")
    endif()

    set(SOURCE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/${MODEL}")
    set(SOURCE_NAME "${MODEL}")
    if(MODEL STREQUAL "SteamVRGlove")
        set(SOURCE_NAME "steamvr_glove_l")
    endif()

    if(EXISTS "${SOURCE_DIRECTORY}/glTF-Binary/${SOURCE_NAME}.glb")
        set(SOURCE_FILE "${SOURCE_DIRECTORY}/glTF-Binary/${SOURCE_NAME}.glb")
    else()
        set(SOURCE_FILE "${SOURCE_DIRECTORY}/glTF/${SOURCE_NAME}.gltf")
    endif()

    # Keep each model's attribution beside its prepared copy
    set(ATTRIBUTION_FILES)
    foreach(ATTRIBUTION README.md LICENSE.md)
        if(EXISTS "${SOURCE_DIRECTORY}/${ATTRIBUTION}")
            list(APPEND ATTRIBUTION_FILES "${SOURCE_DIRECTORY}/${ATTRIBUTION}")
        endif()
    endforeach()

    foreach(ASSET_TARGET preprocess_assets regenerate_assets)
        set(FORCE_ARGUMENT)
        if(ASSET_TARGET STREQUAL "regenerate_assets")
            set(FORCE_ARGUMENT --force)
        endif()

        add_custom_command(TARGET ${ASSET_TARGET} POST_BUILD
            COMMAND xrvk-tools gltf
                --source "${SOURCE_FILE}"
                --output "${OUTPUT_DIRECTORY}" --ktx "${KTX_EXECUTABLE}" --encode astc ${FORCE_ARGUMENT}
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                ${ATTRIBUTION_FILES} "${OUTPUT_DIRECTORY}"
            VERBATIM USES_TERMINAL)
    endforeach()
endforeach()

add_dependencies(preprocess_assets xrvk-tools)
add_dependencies(regenerate_assets xrvk-tools)

# Prepared studio and night grid lighting are bundled, regeneration uses the xrvk CPU baker
add_custom_target(regenerate_environment
    COMMAND xrvk-tools environment
        --source "${CMAKE_CURRENT_SOURCE_DIR}/src/Studio/studio_small_09_1k.hdr"
        --output "${CMAKE_CURRENT_SOURCE_DIR}/bin/PreparedStudio/studio.ibl"
        --diffuse-size 32 --specular-size 256 --lut-size 128 --samples 1024
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/bin/PreparedNightGrid"
    COMMAND xrvk-tools night-grid
        --ibl "${CMAKE_CURRENT_SOURCE_DIR}/bin/PreparedNightGrid/night_grid.ibl"
        --sky "${CMAKE_CURRENT_SOURCE_DIR}/bin/PreparedNightGrid/night_grid.sky"
        --width 2048 --floor-size 4.5 --diffuse-size 32 --specular-size 256 --lut-size 128 --samples 1024
        --star-reflections 10
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/Studio/ATTRIBUTION.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/PreparedStudio/ATTRIBUTION.txt"
    VERBATIM USES_TERMINAL)

add_dependencies(regenerate_environment xrvk-tools)
