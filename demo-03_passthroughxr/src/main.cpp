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


#include <xrapp.hpp>
#include <xrlib/ext/FB/passthrough.hpp>

#define APP_NAME "passthroughxr"
using namespace xrapp;

#ifdef XR_USE_PLATFORM_ANDROID
void android_main( struct android_app *pAndroidApp )
{
	// (1) Create Base XrApp
	std::unique_ptr< XrApp > pApp = std::make_unique< XrApp >( 
		pAndroidApp, 
		APP_NAME,
		XR_MAKE_VERSION32( 1, 0, 0 ), 
		ELogLevel::LogVerbose );
#else
int main( int argc, char *argv[] )
{
	// (1) Create Base XrApp
	std::unique_ptr< XrApp > pApp = std::make_unique< XrApp >( argc, argv, APP_NAME, XR_MAKE_VERSION32( 1, 0, 0 ), ELogLevel::LogVerbose );
#endif

	// (2) Define api layers and extensions our app needs
	std::vector< const char * > vecAPILayers;
	std::vector< const char * > vecRequiredExtensions = { 
		XR_KHR_VULKAN_ENABLE_EXTENSION_NAME, 
		XR_KHR_VISIBILITY_MASK_EXTENSION_NAME,
		XR_FB_PASSTHROUGH_EXTENSION_NAME,			// Passthrough extension our app will use
		XR_FB_TRIANGLE_MESH_EXTENSION_NAME };		// Optional, used to project to passthrough to a mesh, not shown in this demo

	// (3) Initialize openxr instance
	if ( !XR_UNQUALIFIED_SUCCESS( pApp->InitInstance( vecRequiredExtensions, vecAPILayers ) ) )
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif

	// (4) Initialize openxr session
	SSessionSettings defaultSessionSettings;
	if ( !XR_UNQUALIFIED_SUCCESS( pApp->InitSession( defaultSessionSettings ) ) )
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif

	// (5) Check for required extensions our app needs
	if ( !pApp->GetInstance()->IsExtensionEnabled( XR_FB_PASSTHROUGH_EXTENSION_NAME ) )
	{
		LogError( pApp->GetInstance()->GetAppName(), 
			"The current openxr runtime does not support an extension this app needs: %s \n", XR_FB_PASSTHROUGH_EXTENSION_NAME  );
            #ifdef XR_USE_PLATFORM_ANDROID
                return xrlib::ExitApp( pAndroidApp );
            #else
                return xrlib::ExitApp( EXIT_FAILURE );
            #endif
	}

	// (6) Create FB Passthrough
	std::unique_ptr< FB::CPassthrough > pPassthrough = std::make_unique< FB::CPassthrough >( pApp->GetInstance()->GetXrInstance() );

	if ( !XR_UNQUALIFIED_SUCCESS( pPassthrough->Init( pApp->GetSession()->GetXrSession(), pApp->GetInstance() ) ) )
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif

	LogInfo( pApp->GetInstance()->GetAppName(), "User's device supports passthrough: %s", pPassthrough->BSystemSupportsPassthrough() ? "true" : "false" );
	LogInfo( pApp->GetInstance()->GetAppName(), "User's device supports color passthrough: %s", pPassthrough->BSystemSupportsColorPassthrough() ? "true" : "false" );

	// (7) Create passthrough layer
	if ( !XR_UNQUALIFIED_SUCCESS( pPassthrough->AddLayer( 
		pApp->GetSession()->GetXrSession(), 
		ExtBase_Passthrough::ELayerType::FULLSCREEN, 
		XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT, 
		XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB ) ) )
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif

	pPassthrough->Start();

	// (8) Initialize renderer
	if ( !XR_UNQUALIFIED_SUCCESS( pApp->InitRender( { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB }, { VK_FORMAT_D24_UNORM_S8_UINT } ) ) )
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif

	// (9) Create render pass
	if ( !XR_UNQUALIFIED_SUCCESS( pApp->CreateMainRenderPass() ) )
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif

	// (10) Create graphics pipelines
	if ( pApp->BUseVisVMask() )
		pApp->CreatePipeline_VisMask( pApp->defaultShaders.vismaskVertexShaders, pApp->defaultShaders.vismaskFragmentShaders );

	uint32_t layoutIndex = pApp->pRenderInfo->AddNewLayout();
	uint32_t pipelineIndex = pApp->pRenderInfo->AddNewPipeline();
	pApp->CreatePipeline_Primitives( layoutIndex, pipelineIndex, pApp->defaultShaders.primitivesVertexShader, pApp->defaultShaders.primitivesFragmentShader );

	// (11) Create vis masks
	pApp->CreateVismasks();

	// (12) Setup the scene to render
	
	// Floor
	{
		auto *pFloor = new CColoredCube( pApp->GetSession(), pApp->pRenderInfo.get(), true, { 1.f, 0.1f, 1.f } );
		pFloor->instances[ 0 ].pose = { { 0.f, 0.f, 0.f, 1.f }, { 0.0f, 0.f, 0.f } };
		pFloor->Recolor( { 1.f, 1.f, 0.f }, 0.75f );
		pFloor->InitBuffers();
		pApp->pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( pFloor ) );
	}

	// (13) Render loop
	while ( pApp->GetSession()->GetState() != XR_SESSION_STATE_EXITING )
	{
		// (13.1) Poll for xr events
		XrEventDataBaseHeader xrEventDataBaseheader { XR_TYPE_EVENT_DATA_EVENTS_LOST };
		if ( !XR_SUCCEEDED( pApp->GetSession()->Poll( &xrEventDataBaseheader ) ) )
			continue;

		// (13.2) Process xr events
		pApp->ProcessEvents_SessionState( xrEventDataBaseheader );
		pApp->ProcessEvents_Vismasks( xrEventDataBaseheader );

		// (13.3) Render frame
		if ( pApp->GetSession()->GetState() >= XR_SESSION_STATE_READY )
		{
			// (13.3.1) Add passthrough layers
			if ( pPassthrough->IsActive() )
			{
				pApp->pRenderInfo->state.compositionLayerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
				pApp->pRenderInfo->state.clearValues[ 0 ].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

                if( pApp->pRenderInfo->state.preAppFrameLayers.empty() )
				    pPassthrough->GetCompositionLayers( pApp->pRenderInfo->state.preAppFrameLayers, false );
			} 
			else
			{
				pApp->pRenderInfo->state.compositionLayerFlags = 0;
				pApp->pRenderInfo->state.clearValues[ 0 ].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
			}
			

			// (13.3.2) Render frame
			pApp->GetRender()->RenderFrame( pApp->mainRenderPass, pApp->pRenderInfo.get(), pApp->vecMasks );
		}
	}

	// (14) Exit app - xrlib objects (instance, session, renderer, etc) handles proper cleanup once unique pointers goes out of scope.
	//				  Note that as they are in the same scope in this demo, order of destruction here is automatically enforced only when using C++20 and above
    #ifdef XR_USE_PLATFORM_ANDROID
        return xrlib::ExitApp( pAndroidApp );
    #else
        return xrlib::ExitApp( EXIT_FAILURE );
    #endif
}
