// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0

/**
 * @file
 * @brief Sample entry point for the native visionOS shell
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief Poll compositor lifecycle between frames on the dedicated render thread
	 * @param[in] context Borrowed native host, retained throughout the sample loop
	 * @return False when the immersive layer is invalidated, may wait while paused
	 */
	typedef bool ( *SampleHostPoll )( void *context );

	/** @brief Run the glTF viewer using OpenXR and PBR rendering
	 * @param[in] pollHost Native lifecycle callback, required
	 * @param[in] context Borrowed native host
	 * @return OpenXR result
	 * @pre The public SDK host is attached and the working directory contains compiled shaders and model assets
	 */
	int32_t RunGltfXr( SampleHostPoll pollHost, void *context );
#ifdef __cplusplus
}
#endif
