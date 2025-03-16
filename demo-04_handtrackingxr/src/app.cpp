
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


#include <app.hpp>

namespace app
{

#ifdef XR_USE_PLATFORM_ANDROID

	App::App( struct android_app *pAndroidApp, const std::string &sAppName, const XrVersion32 unAppVersion, const ELogLevel eMinLogLevel )
		: XrApp( pAndroidApp, sAppName, unAppVersion, eMinLogLevel )
	{

	}
#else
	App::App( int argc, char *argv[], const std::string &sAppName, const XrVersion32 unAppVersion, const ELogLevel eMinLogLevel )
		: XrApp( argc, argv, sAppName, unAppVersion, eMinLogLevel )
	{

	}
#endif

	App::~App() 
	{ 
	}

#ifdef XR_USE_PLATFORM_ANDROID
	void App::Init()
#else
	int App::Init() 
#endif
	{ 
		// (1) Define api layers and extensions our app needs
		std::vector< const char * > vecAPILayers;
		std::vector< const char * > vecRequiredExtensions = {
			XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,
			XR_KHR_VISIBILITY_MASK_EXTENSION_NAME,
			XR_EXT_HAND_TRACKING_EXTENSION_NAME,		// Handtracking extension
			XR_FB_PASSTHROUGH_EXTENSION_NAME,			// Passthrough extension our app will use
			XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME,  // Display refresh rate, for consistent animations across devices
			XR_FB_TRIANGLE_MESH_EXTENSION_NAME };		// Optional, used to project to passthrough to a mesh, not shown in this demo

		// (2) Initialize openxr instance
		if ( !XR_UNQUALIFIED_SUCCESS( InitInstance( vecRequiredExtensions, vecAPILayers ) ) )
            #ifdef XR_USE_PLATFORM_ANDROID
            return;
            #else
            return EXIT_FAILURE;
            #endif

		// (3) Initialize openxr session
		SSessionSettings defaultSessionSettings;
		if ( !XR_UNQUALIFIED_SUCCESS( InitSession( defaultSessionSettings ) ) )
            #ifdef XR_USE_PLATFORM_ANDROID
            return;
            #else
            return EXIT_FAILURE;
            #endif

		// (4) Initialize FB Passthrough
		if ( GetPassthrough() )
		{
			// (5.1) Initialize passthrough
			if ( !XR_UNQUALIFIED_SUCCESS( InitPassthrough() ) )
                #ifdef XR_USE_PLATFORM_ANDROID
                return;
                #else
                return EXIT_FAILURE;
                #endif

			// (5.2) Create passthrough layer
			if ( !XR_UNQUALIFIED_SUCCESS(
					 GetPassthrough()->AddLayer( GetSession()->GetXrSession(), ExtBase_Passthrough::ELayerType::FULLSCREEN, XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT, XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB ) ) )
                    #ifdef XR_USE_PLATFORM_ANDROID
                        return;
                    #else
                        return EXIT_FAILURE;
                    #endif

			// (5.3) Start passthrough
			GetPassthrough()->Start();
		}


		// (5) Initialize renderer
		if ( !XR_UNQUALIFIED_SUCCESS( InitRender( { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB }, { VK_FORMAT_D24_UNORM_S8_UINT } ) ) )
            #ifdef XR_USE_PLATFORM_ANDROID
            return;
            #else
        return EXIT_FAILURE;
            #endif

		// (6) Create render pass
		if ( !XR_UNQUALIFIED_SUCCESS( CreateMainRenderPass() ) )
            #ifdef XR_USE_PLATFORM_ANDROID
            return;
            #else
            return EXIT_FAILURE;
            #endif

		// (7) Create graphics pipelines
		CreateGraphicsPipelines();

		// (8) Create vis masks
		CreateVismasks();

        #ifndef XR_USE_PLATFORM_ANDROID
                std::cout << "\n\nPress enter to end.";
                std::cin.get();

                return EXIT_SUCCESS;
        #endif
	}

	void App::SetupScene() 
	{
		// Floor
		//vecPrimitives.push_back( new CColoredCube( GetSession(), 0, { 5.f, 0.1f, 5.f }, EAssetMotionType::STATIC_INIT, 1.f ) );
		//vecPrimitives.back()->instances[ 0 ].pose = { { 0.f, 0.f, 0.f, 1.f }, { 0.0f, 0.f, 0.f } };
		//vecPrimitives.back()->Recolor( { 1.f, 1.f, 0.f }, 0.25f );
		//vecPrimitives.back()->InitBuffers();

	}

	void App::ProcessXrEvents( XrEventDataBaseHeader &xrEventDataBaseheader ) 
	{
		ProcessEvents_SessionState( xrEventDataBaseheader );
		ProcessEvents_Vismasks( xrEventDataBaseheader );
	}

	bool App::StartRenderFrame() 
	{
		if ( GetSession()->GetState() >= XR_SESSION_STATE_READY )
		{
			// Set up state before submitting to render thread
			if ( GetPassthrough() && GetPassthrough()->IsActive() )
			{
				pRenderInfo->state.compositionLayerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
				pRenderInfo->state.clearValues[ 0 ].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
				GetPassthrough()->GetCompositionLayers( pRenderInfo->state.preAppFrameLayers );
			}
			else
			{
				pRenderInfo->state.compositionLayerFlags = 0;
				pRenderInfo->state.clearValues[ 0 ].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
			}

			// Submit actual render work to render thread
			return GetRender()->StartRenderFrame( pRenderInfo.get() );
		}

		return false;
	}

	void App::EndRenderFrame() 
	{ 
		GetRender()->EndRenderFrame( mainRenderPass, pRenderInfo.get(), vecMasks ); 
	}

	void App::CreateGraphicsPipelines() 
	{ 
		// Vismask
		if ( m_pVisMask )
			CreatePipeline_VisMask( defaultShaders.vismaskVertexShaders, defaultShaders.vismaskFragmentShaders );

		// App graphics pipelines
		pipelines.primitiveLayout = pRenderInfo->AddNewLayout();

		pipelines.primitives = pRenderInfo->AddNewPipeline();
		CreatePipeline_Primitives( pipelines.primitiveLayout, pipelines.primitives, defaultShaders.primitivesVertexShader, defaultShaders.primitivesFragmentShader );

	}

} // namespace app
