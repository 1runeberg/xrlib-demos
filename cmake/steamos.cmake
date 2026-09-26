# Copyright 2026 Rune Berg
# Licensed under Apache 2.0
# SPDX-License-Identifier: Apache-2.0

set(STEAMOS_PROJECT "" CACHE STRING "Demo to build for SteamOS ARM64")
set(STEAMOS_DEMOS demo-01_checkxr demo-02_displayxr demo-05_inputxr)

if(NOT STEAMOS_PROJECT IN_LIST STEAMOS_DEMOS)
    message(FATAL_ERROR "Set STEAMOS_PROJECT to demo-01_checkxr, demo-02_displayxr or demo-05_inputxr")
endif()

add_subdirectory("${STEAMOS_PROJECT}")
