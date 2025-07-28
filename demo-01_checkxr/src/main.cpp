/*
 * Copyright 2024-25 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
 */


#include <iostream>
#include <memory>

#include <xrlib.hpp>

using namespace xrlib;

/// Create an openxr instance and probe the user's system
/// for installed api layers and supported extensions from
/// the active openxr runtime.

#ifdef XR_USE_PLATFORM_ANDROID
void android_main( struct android_app *pAndroidApp )
{
    // (1a) Create an xr instance, we'll leave the optional log level parameter to verbose.
    std::unique_ptr< CInstance > pXrInstance = std::make_unique< CInstance >( pAndroidApp, "checkxr", 1 );

    // (1b) Initialize android openxr loader
    if ( !XR_UNQUALIFIED_SUCCESS( pXrInstance->InitAndroidLoader( pAndroidApp ) ) )
        return xrlib::ExitApp( pAndroidApp );

#else
	int main( int argc, char *argv[] )
	{
		// (1) Create an xr instance, we'll leave the optional log level parameter to verbose.
		std::unique_ptr< CInstance > pXrInstance = std::make_unique< CInstance >( "checkxr", 1 );
#endif
	
		// (2) Probe user's system for installed api layers
		std::vector< std::string > vecAvailableApiLayers;
		if ( !XR_UNQUALIFIED_SUCCESS( pXrInstance->GetSupportedApiLayers( vecAvailableApiLayers ) ) )
            #ifdef XR_USE_PLATFORM_ANDROID
                return xrlib::ExitApp( pAndroidApp );
            #else
                return xrlib::ExitApp( EXIT_FAILURE );
            #endif

		LogInfo("API LAYERS", "Found %i api layers in this system.", vecAvailableApiLayers.size() );
		for ( uint32_t i = 0; i < vecAvailableApiLayers.size(); i++ )
			LogInfo( std::to_string( i ), "%s", vecAvailableApiLayers[ i ].c_str() );

		// (3) Probe openxr runtime for installed extensions
		std::vector< XrExtensionProperties > vecAvailableExtensions;
		if ( !XR_UNQUALIFIED_SUCCESS( pXrInstance->GetSupportedExtensions( vecAvailableExtensions ) ) )
            #ifdef XR_USE_PLATFORM_ANDROID
                return xrlib::ExitApp( pAndroidApp );
            #else
                return xrlib::ExitApp( EXIT_FAILURE );
            #endif

		LogInfo( "EXTENSIONS", "Found %i supported extensions on active openxr runtime.", vecAvailableExtensions.size() );
		for ( size_t i = 0; i < vecAvailableExtensions.size(); i++ )
			LogInfo( std::to_string( i ), "%s (ver. %i)", &vecAvailableExtensions[ i ].extensionName, vecAvailableExtensions[ i ].extensionVersion );

		// (4) Exit app - CInstance handles proper cleanup once unique pointer goes out of scope.
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif
	}
