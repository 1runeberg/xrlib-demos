/* 
 * Copyright 2024,2025 Copyright Rune Berg 
 * https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This work is the next iteration of OpenXRProvider (v1, v2)
 * OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
 * OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
 * v1 & v2 licensed under MIT: https://opensource.org/license/mit
*/


#include <iostream>
#include <memory>

#include <xrlib.hpp>
#include <xrlib/ext/KHR/visibility_mask.hpp>	// Stencil out portions of the eye textures that will never be visible to the user
#include <xrvk/render.hpp>						// We'll use xrlib's built-in vulkan renderer

using namespace xrlib;

/// Create an openxr instance and probe the user's system
/// for installed api layers and supported extensions from
/// the active openxr runtime.

#ifdef XR_USE_PLATFORM_ANDROID
	void android_main( struct android_app *pAndroidApp )
	{
		// (1a) Create an xr instance, we'll leave the optional log level parameter to verbose.
		std::unique_ptr< CInstance > pXrInstance = std::make_unique< CInstance >( pAndroidApp, "displayxr", 1 );

        // (1b) Initialize android openxr loader
        if ( !XR_UNQUALIFIED_SUCCESS( pXrInstance->InitAndroidLoader( pAndroidApp ) ) )
            return xrlib::ExitApp( pAndroidApp );
#else
	int main( int argc, char *argv[] )
	{
		// (1) Create an xr instance, we'll leave the optional log level parameter to verbose.
		std::unique_ptr< CInstance > pXrInstance = std::make_unique< CInstance >( "displayxr", 1 );
#endif
	
		// (2) Set required extensions we need to run our app
		std::vector< const char * > vecRequiredExtensions = { 
			XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,	// this is the graphics api we will use - xrlib only supports vulkan at this time
			XR_KHR_VISIBILITY_MASK_EXTENSION_NAME	// this helps our app improve rendering performance by stenciling out portions of the screen that the user can't see based on their hmd lenses
		};

		// (3) Check that the user's runtime can run our app
        LogError( "xrlib", "This app requires the following openxr extensions: " );
		for ( auto ext : vecRequiredExtensions ) 
			std::cout << ext << std::endl;

		if( !XR_UNQUALIFIED_SUCCESS( pXrInstance->RemoveUnsupportedExtensions(vecRequiredExtensions) ) )
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif

		if ( vecRequiredExtensions.size() != vecRequiredExtensions.size() )
		{
            LogError( "xrlib", "FATAL: Current openxr runtime does not support all required extensions. Only the following was found:" );
			for ( auto ext : vecRequiredExtensions ) 
				std::cout << ext << std::endl;

            #ifdef XR_USE_PLATFORM_ANDROID
                return xrlib::ExitApp( pAndroidApp );
            #else
                return xrlib::ExitApp( EXIT_FAILURE );
            #endif
		}
		
		// (4) Now we're ready to initialize an openxr instance
		std::vector< const char * > vecAPILayers;
		if( !XR_UNQUALIFIED_SUCCESS( pXrInstance->Init( vecRequiredExtensions, vecAPILayers , 0, nullptr ) ) )
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif

        #ifdef XR_USE_PLATFORM_ANDROID
		// For android, we need to pass in addition the android state management function to the xrProvider
        pAndroidApp->onAppCmd = app_handle_cmd;
        #endif

		// (5) Create and initialize openxr session
		std::unique_ptr< CSession > pXrSession = std::make_unique< CSession >( pXrInstance.get() );
		SSessionSettings defaultSessionSettings;
		if( !XR_UNQUALIFIED_SUCCESS( pXrSession->Init( defaultSessionSettings ) ) )
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif

		// (6) Handle extensions
		std::unique_ptr< KHR::CVisibilityMask > pVisMask = nullptr;
		if ( pXrInstance->IsExtensionEnabled( XR_KHR_VISIBILITY_MASK_EXTENSION_NAME ) )
		{
			pVisMask = std::make_unique< KHR::CVisibilityMask >( pXrInstance->GetXrInstance() );
		}

		// (7) Set texture formats our app's renderer will use
		VkFormat vkFormatColor = (VkFormat) pXrSession->SelectColorTextureFormat( { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB } );
		VkFormat vkFormatDepth = (VkFormat) pXrSession->SelectDepthTextureFormat( { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT } );

		if (vkFormatColor == 0 || vkFormatDepth == 0)
		{
			// Our app's needed texture formats aren't supported.
            LogError( "xrlib", "FATAL: Current openxr runtime does not support the texture formats required by this app." );
            #ifdef XR_USE_PLATFORM_ANDROID
                return xrlib::ExitApp( pAndroidApp );
            #else
                return xrlib::ExitApp( EXIT_FAILURE );
            #endif
		}

		// (8) Create and initialize xrlib's built-in vulkan stereo renderer for our xr app
		std::unique_ptr< CStereoRender > pRender = std::make_unique< CStereoRender >( pXrSession.get(), vkFormatColor, vkFormatDepth );
		pRender->Init();

		// (9) Create main render pass
		VkRenderPass renderPass = VK_NULL_HANDLE;
		pRender->CreateRenderPass( renderPass, pVisMask != nullptr );

		// (10) Create graphics pipelines to use for rendering
		//     We'll use 2 if visibility mask is supported by the openxr runtime
		
		VkResult vkResult = VK_SUCCESS;
		std::unique_ptr< CRenderInfo > pRenderInfo = std::make_unique< CRenderInfo >( pXrSession.get() );

		// (10.1) Create stencil pipeline - for vis mask
		if ( pRender->GetUseVisMask() )
		{
			vkResult = pRender->CreateGraphicsPipeline_Stencils(
				#ifdef XR_USE_PLATFORM_ANDROID
					pAndroidApp->activity->assetManager,
				#endif
					pRenderInfo->stencilLayout,
					pRenderInfo->stencilPipelines,
					renderPass,
					{ "stencil0.vert.spv", "stencil1.vert.spv" },
					{ "stencil.frag.spv", "stencil.frag.spv" } ); 

			assert( VK_CHECK_SUCCESS( vkResult ) );
		}
		
		// (10.2) Create render pipeline
		uint32_t layoutIdx = pRenderInfo->AddNewLayout();
		uint32_t pipelineIdx = pRenderInfo->AddNewPipeline();

		vkResult = pRender->CreateGraphicsPipeline_Primitives(
			#ifdef XR_USE_PLATFORM_ANDROID
				pAndroidApp->activity->assetManager,
			#endif
			pRenderInfo->vecPipelineLayouts[ layoutIdx ],
			pRenderInfo->vecGraphicsPipelines[ pipelineIdx ],
			renderPass,
			"primitive.vert.spv",
			"primitive.frag.spv" ); 

		assert( VK_CHECK_SUCCESS( vkResult ) );

		// (10.3) Setup vis mask
		std::vector< CPlane2D * > vecMasks;
		if ( pVisMask )
		{
			// Retrieve left eye vismask and convert to a Plane2D
			vecMasks.push_back( new CPlane2D( pXrSession.get(),	pRenderInfo.get(),	0,	0 ) );
			
			pVisMask->GetVisMaskShortIndices(
				pXrSession->GetXrSession(), 
				*vecMasks.back()->GetVertices(),
				*vecMasks.back()->GetIndices(),
				XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
				k_Left,
				XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
			vecMasks.back()->InitBuffers();

			// Retrieve right eye vismask and convert to a Plane2D
			vecMasks.push_back( new CPlane2D( pXrSession.get(), pRenderInfo.get(), 0, 0 ) );
			pVisMask->GetVisMaskShortIndices(
				pXrSession->GetXrSession(), 
				*vecMasks.back()->GetVertices(),
				*vecMasks.back()->GetIndices(),
				XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
				k_Right,
				XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
			vecMasks.back()->InitBuffers();
		}
		
		// (11) Setup the scene to render

		// Floor
		{
			auto *pFloor = new CColoredCube( pXrSession.get(), pRenderInfo.get(), true, { 2.f, 0.1f, 2.f } );
			pFloor->instances[ 0 ].pose = { { 0.f, 0.f, 0.f, 1.f }, { 0.0f, 0.f, 0.f } };
			pFloor->Recolor( { 0.25f, 0.25f, 0.25f }, 0.75f );
			pFloor->InitBuffers();
			pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( pFloor ) );
		}

		// Axis indicators
		{
			auto *pAxisIndicator = new CColoredPyramid( pXrSession.get(), pRenderInfo.get(), true, { 0.45f, 0.45f, 0.45f } );
			
			pAxisIndicator->AddInstance( 3, { 0.25f, 0.25f, 0.25f } );	 // add three more instances so we have one for each cardinal direction
			pAxisIndicator->instances[ 0 ].pose = { { 0.f, 0.f, 0.f, 1.f }, { 0.0f, 0.25f, -1.f } };			   // North
			pAxisIndicator->instances[ 1 ].pose = { { 0.f, 1.f, 0.f, 0.f }, { 0.0f, 0.25f, 1.f } };				   // South
			pAxisIndicator->instances[ 2 ].pose = { { 0.f, -0.7071f, 0.f, 0.7071f }, { 1.0f, 0.25f, 0.f } };	   // East
			pAxisIndicator->instances[ 3 ].pose = { { 0.f, 0.7071f, 0.f, 0.7071f }, { -1.0f, 0.25f, 0.f } };	   // West

			pAxisIndicator->InitBuffers();
			pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( pAxisIndicator ) );
		}

		// (12) Render loop
		while ( pXrSession->GetState() != XR_SESSION_STATE_EXITING )
		{
			// (12.1) Poll for xr events
			XrEventDataBaseHeader xrEventDataBaseheader { XR_TYPE_EVENT_DATA_EVENTS_LOST };
			if ( !XR_SUCCEEDED( pXrSession->Poll( &xrEventDataBaseheader ) ) )
				continue;

			// (12.2) Process xr events
			if ( xrEventDataBaseheader.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED )
			{
				if ( pXrSession->GetState() == XR_SESSION_STATE_READY )
				{
					// Start session - begin the app's frame loop here
					if ( !XR_UNQUALIFIED_SUCCESS( pXrSession->Start() ) )
						continue;

					LogInfo( "", "OpenXR session started." );
				}
				else if ( pXrSession->GetState() == XR_SESSION_STATE_STOPPING )
				{
					// End session - end the app's frame loop here
					if ( !XR_UNQUALIFIED_SUCCESS( pXrSession->End() ) )
						continue;

					LogInfo( "", "OpenXR session exiting." );
				}
			} 

			if ( xrEventDataBaseheader.type == XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR 
					&& pVisMask
					&& vecMasks.size() == 2 
				)
			{
				pVisMask->UpdateVisMaskShortIndices(
					xrEventDataBaseheader, 
					pXrSession->GetXrSession(),
					*vecMasks.at( 0 )->GetVertices(),
					*vecMasks.at( 0 )->GetIndices(),
					XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
					k_Left,
					XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
				vecMasks.at( 0 )->InitBuffers();

				pVisMask->UpdateVisMaskShortIndices(
					xrEventDataBaseheader,
					pXrSession->GetXrSession(),
					*vecMasks.at( 1 )->GetVertices(),
					*vecMasks.at( 1 )->GetIndices(),
					XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
					k_Right,
					XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
				vecMasks.at( 1 )->InitBuffers();

			}


			// (12.3) Render frame
			if ( pXrSession->GetState() >= XR_SESSION_STATE_READY )
			{
				std::vector<CRenderModel *> vecScenes;
				pRender->RenderFrame( renderPass, pRenderInfo.get(), vecMasks );
			}
		} 

		// (13) Exit app - xrlib objects (instance, session, renderer, etc) handles proper cleanup once unique pointers goes out of scope.
		//				  Note that as they are in the same scope in this demo, order of destruction here is automatically enforced only when using C++20 and above
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif
	}
