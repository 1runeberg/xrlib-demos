# Copyright 2026 Rune Berg
# Licensed under Apache 2.0
# SPDX-License-Identifier: Apache-2.0

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    message(FATAL_ERROR "SteamOS requires a Linux ARM64 build environment")
endif()

get_filename_component(APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(DEMOS_ROOT "${APP_ROOT}/.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET xrlib)
    set(XRLIB_SOURCE "${DEMOS_ROOT}/xrlib" CACHE PATH "xrlib source repo")
    set(XRLIB_ROOT "${CMAKE_BINARY_DIR}/xrlib-source")

    # xrlib generates files in its source tree, keep that work in this demo's build
    file(GLOB_RECURSE XRLIB_FILES CONFIGURE_DEPENDS LIST_DIRECTORIES false "${XRLIB_SOURCE}/*")
    foreach(SOURCE_FILE IN LISTS XRLIB_FILES)
        file(RELATIVE_PATH RELATIVE_FILE "${XRLIB_SOURCE}" "${SOURCE_FILE}")
        if(RELATIVE_FILE MATCHES "(^|/)(\\.git|build|cmake-build-[^/]*)(/|$)" OR
           RELATIVE_FILE MATCHES "^(bin|lib)/")
            continue()
        endif()

        configure_file("${SOURCE_FILE}" "${XRLIB_ROOT}/${RELATIVE_FILE}" COPYONLY)
    endforeach()

    set(BUILD_AS_STATIC OFF CACHE BOOL "Build xrlib as a shared library" FORCE)
    set(BUILD_SHADERS ON CACHE BOOL "Compile shaders" FORCE)
    set(ENABLE_XRVK ON CACHE BOOL "Compile xrvk" FORCE)
    set(ENABLE_RENDERDOC OFF CACHE BOOL "Enable RenderDoc" FORCE)
    set(ENABLE_VULKAN_DEBUG OFF CACHE BOOL "Enable Vulkan debugging" FORCE)
    set(DYNAMIC_LOADER OFF CACHE BOOL "Embed the OpenXR loader in xrlib" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "Build OpenXR tests" FORCE)
    set(BUILD_API_LAYERS OFF CACHE BOOL "Build OpenXR API layers" FORCE)
    add_subdirectory("${XRLIB_ROOT}" "${CMAKE_BINARY_DIR}/xrlib")

    # Carry over the system libraries needed by the embedded loader
    target_link_libraries(xrlib PRIVATE openxr_loader)
    set_target_properties(xrlib PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

get_target_property(XRLIB_ROOT xrlib SOURCE_DIR)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${APP_ROOT}/src/*.cpp")
if(APP_NAME STREQUAL "inputxr")
    file(GLOB XRAPP_SOURCES CONFIGURE_DEPENDS "${DEMOS_ROOT}/xrapp/*.cpp")
    list(APPEND APP_SOURCES ${XRAPP_SOURCES})
endif()

add_executable(${APP_NAME} ${APP_SOURCES})
target_include_directories(${APP_NAME} PRIVATE "${APP_ROOT}/src" "${DEMOS_ROOT}/xrapp")
target_link_libraries(${APP_NAME} PRIVATE xrlib)
set_target_properties(${APP_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    INSTALL_RPATH "$ORIGIN")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/deploy" CACHE PATH "Deployment folder" FORCE)
endif()

install(TARGETS ${APP_NAME} xrlib
    RUNTIME DESTINATION . COMPONENT SteamOS
    LIBRARY DESTINATION . COMPONENT SteamOS)
configure_file("${DEMOS_ROOT}/platforms/steamos/launch.sh.in" "${CMAKE_CURRENT_BINARY_DIR}/launch.sh" @ONLY)
install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/launch.sh" DESTINATION . COMPONENT SteamOS)
install(FILES "${DEMOS_ROOT}/LICENSE" DESTINATION licenses COMPONENT SteamOS RENAME xrlib-demos.txt)
install(FILES "${XRLIB_ROOT}/LICENSE" DESTINATION licenses COMPONENT SteamOS RENAME xrlib.txt)
install(FILES "${XRLIB_ROOT}/third_party/fastgltf/LICENSE.md"
    DESTINATION licenses COMPONENT SteamOS RENAME fastgltf.txt)
foreach(DEPENDENCY simdjson stb)
    install(FILES "${XRLIB_ROOT}/third_party/${DEPENDENCY}/LICENSE"
        DESTINATION licenses COMPONENT SteamOS RENAME "${DEPENDENCY}.txt")
endforeach()
install(DIRECTORY "${XRLIB_ROOT}/third_party/openxr/LICENSES/"
    DESTINATION licenses/openxr COMPONENT SteamOS)
install(FILES "${XRLIB_ROOT}/third_party/openxr/COPYING.adoc"
    DESTINATION licenses/openxr COMPONENT SteamOS)

if(NOT APP_NAME STREQUAL "checkxr")
    install(DIRECTORY "${XRLIB_ROOT}/res/shaders/bin/" DESTINATION . COMPONENT SteamOS
        FILES_MATCHING PATTERN "*.spv")
endif()

if(APP_NAME STREQUAL "inputxr")
    file(GLOB APP_SHADERS CONFIGURE_DEPENDS "${APP_ROOT}/assets/src/*.frag" "${APP_ROOT}/assets/src/*.vert")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    set(SHADER_OUTPUTS)
    foreach(SHADER IN LISTS APP_SHADERS)
        get_filename_component(SHADER_NAME "${SHADER}" NAME)
        set(SHADER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
        add_custom_command(OUTPUT "${SHADER_OUTPUT}"
            COMMAND glslc "${SHADER}" -o "${SHADER_OUTPUT}"
            DEPENDS "${SHADER}" VERBATIM)
        list(APPEND SHADER_OUTPUTS "${SHADER_OUTPUT}")
    endforeach()

    add_custom_target(inputxr_shaders DEPENDS ${SHADER_OUTPUTS})
    add_dependencies(inputxr inputxr_shaders)
    install(FILES ${SHADER_OUTPUTS} DESTINATION . COMPONENT SteamOS)
    install(DIRECTORY "${XRLIB_ROOT}/res/models/bin/" "${APP_ROOT}/assets/bin/"
        DESTINATION . COMPONENT SteamOS
        PATTERN "*.spv" EXCLUDE PATTERN ".DS_Store" EXCLUDE)
endif()
