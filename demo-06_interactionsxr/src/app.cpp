/* 
 * Copyright 2024-26 Copyright Rune Berg 
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
		ReleaseBackdrop();
	}

	void App::ReleaseBackdrop()
	{
		if ( !m_pXrSession || !m_pXrSession->GetVulkan() || !m_pXrSession->GetVulkan()->GetVkLogicalDevice() )
			return;

		// The sky texture and night lighting belong to the device, so release them while it's still alive
		vkDeviceWaitIdle( m_pXrSession->GetVulkan()->GetVkLogicalDevice() );
		if ( pRenderInfo && assets.pNightLighting )
			pRenderInfo->SetEnvironment( nullptr );

		assets.pNightLighting.reset();
		if ( pTextureManager && assets.skyTexture.image != VK_NULL_HANDLE )
			pTextureManager->DestroyTexture( assets.skyTexture );
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
			XR_EXT_HAND_TRACKING_EXTENSION_NAME,
			XR_EXT_HAND_INTERACTION_EXTENSION_NAME,
			XR_EXT_HAND_JOINTS_MOTION_RANGE_EXTENSION_NAME,
			XR_META_SIMULTANEOUS_HANDS_AND_CONTROLLERS_EXTENSION_NAME,
			XR_FB_PASSTHROUGH_EXTENSION_NAME,	  // Passthrough extension our app will use
			XR_FB_TRIANGLE_MESH_EXTENSION_NAME,	  // Optional, used to project to passthrough to a mesh, not shown in this demo
			XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME // Display refresh rate for consistent animations across devices
			}; 

		// (2) Initialize openxr instance
		if ( !XR_UNQUALIFIED_SUCCESS( InitInstance( vecRequiredExtensions, vecAPILayers ) ) )
            #ifdef XR_USE_PLATFORM_ANDROID
            return;
            #else
            return EXIT_FAILURE;
            #endif

		// (3) Check system hardware capabilities
		CSystemProperties systemProperties;
		systemProperties.AddSystemProperty( XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT );
		systemProperties.AddSystemProperty( XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT );
		systemProperties.AddSystemProperty( XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES_FB );
		systemProperties.AddSystemProperty( XR_TYPE_SYSTEM_SIMULTANEOUS_HANDS_AND_CONTROLLERS_PROPERTIES_META );

		auto *sysProps = m_pXrInstance->GetXrSystemProperties( true, systemProperties.GetNextChain(), false );
		if ( sysProps->next )
		{
			auto *propertyChain = reinterpret_cast< XrBaseOutStructure * >( sysProps->next );
			auto *handTracking = GetStructureFromChain< XrSystemHandTrackingPropertiesEXT >( *propertyChain, XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT );
			auto *eyeGaze = GetStructureFromChain< XrSystemEyeGazeInteractionPropertiesEXT >( *propertyChain, XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT );
			auto *passthrough = GetStructureFromChain< XrSystemPassthroughPropertiesFB >( *propertyChain, XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES_FB );
			auto *simultaneousHands = GetStructureFromChain< XrSystemSimultaneousHandsAndControllersPropertiesMETA >( *propertyChain, XR_TYPE_SYSTEM_SIMULTANEOUS_HANDS_AND_CONTROLLERS_PROPERTIES_META );

			LogInfo( APP_NAME, "System supports hand tracking: %s", handTracking ? ( handTracking->supportsHandTracking ? "true" : "false" ) : "unknown" );
			LogInfo( APP_NAME, "System supports eye gaze interaction: %s", eyeGaze ? ( eyeGaze->supportsEyeGazeInteraction ? "true" : "false" ) : "unknown" );
			LogInfo( APP_NAME, "System supports passthrough: %s", passthrough ? ( passthrough->supportsPassthrough ? "true" : "false" ) : "unknown" );
			LogInfo( APP_NAME, "System supports simultaneous hands and controllers: %s", simultaneousHands ? ( simultaneousHands->supportsSimultaneousHandsAndControllers ? "true" : "false" ) : "unknown" );

			m_pXrInstance->ResetXrSystemPropertyNextChain( true ); // Clear the next chain to avoid memory leaks, and free up the stack memory used here
		}

		// (4) Initialize openxr session
		SSessionSettings defaultSessionSettings;
		if ( !XR_UNQUALIFIED_SUCCESS( InitSession( defaultSessionSettings ) ) )
            #ifdef XR_USE_PLATFORM_ANDROID
            return;
            #else
            return EXIT_FAILURE;
            #endif


		// Initialize Hand Tracking
		if ( GetHandTracking() )
		{
			// (3.1) Initialize hand tracking
			if ( !XR_UNQUALIFIED_SUCCESS( InitHandTracking( nullptr, nullptr ) ) )
				#ifdef XR_USE_PLATFORM_ANDROID
				return;
				#else
				return EXIT_FAILURE;
				#endif

			// Check if we have hand joints motion range support
			if ( m_pXrInstance->IsExtensionEnabled( XR_EXT_HAND_JOINTS_MOTION_RANGE_EXTENSION_NAME ) )
			{
				m_pHandsJointsMotionRange = std::make_unique< EXT::CHandJointsMotionRange >( m_pXrInstance->GetXrInstance() );
			}

			// Check if we have simultaneous hands and controllers support for meta devices
			if ( m_pXrInstance->IsExtensionEnabled( XR_META_SIMULTANEOUS_HANDS_AND_CONTROLLERS_EXTENSION_NAME ) )
			{
				m_pHandsAndControllers = std::make_unique< META::CHandsAndControllers >( m_pXrInstance->GetXrInstance() );
				if ( m_pHandsAndControllers )
					m_pHandsAndControllers->Enable( GetSession()->GetXrSession() );
			}
		}

		// (4) Initialize FB Passthrough
		if ( GetPassthrough() && GetTriangleMesh() )	// This demo uses the fb triangle mesh extension to project the camera feed to a mesh
		{
			// (4.1) Initialize passthrough
			if ( !XR_UNQUALIFIED_SUCCESS( InitPassthrough() ) )
            #ifdef XR_USE_PLATFORM_ANDROID
                return;
            #else
                return EXIT_FAILURE;
            #endif

			// (4.2) Add Triangle Mesh
			GetPassthrough()->SetTriangleMesh( GetTriangleMesh() );

			// (4.3) Create passthrough layer (mesh projection layer)
			if ( !XR_UNQUALIFIED_SUCCESS(
					 GetPassthrough()->AddLayer( GetSession()->GetXrSession(), ExtBase_Passthrough::ELayerType::MESH_PROJECTION, XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT, XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB ) ) )
                    #ifdef XR_USE_PLATFORM_ANDROID
                    return;
                    #else
                    return EXIT_FAILURE;
					#endif

			// (4.4) Add passthrough geometry (plane) to the layer
			m_vecVertices.push_back( { -0.5f, 0.5f, 0.0f } );  // top left
			m_vecVertices.push_back( { 0.5f, 0.5f, 0.0f } );   // top right
			m_vecVertices.push_back( { 0.5f, -0.5f, 0.0f } );  // bottom right
			m_vecVertices.push_back( { -0.5f, -0.5f, 0.0f } ); // bottom left
			m_vecIndices = { 0, 1, 2, 2, 3, 0 };
			XrResult result = GetPassthrough()->AddGeometry(
				m_pXrSession->GetXrSession(),
				GetPassthrough()->GetPassthroughLayers()->at(0).layer,
				m_vecVertices,
				m_vecIndices,
				m_pXrSession->GetAppSpace(),
				0,
				k_windowPose
 			);

			if( !XR_UNQUALIFIED_SUCCESS( result ) )
			#ifdef XR_USE_PLATFORM_ANDROID
				return;
			#else
				return EXIT_FAILURE;
			#endif

			// (4.5) Start passthrough layers
			result = GetPassthrough()->ResumeLayer();
			LogInfo( APP_NAME, "Starting passthrough layer: %s", XR_UNQUALIFIED_SUCCESS( result ) ? "Success" : "Failed" );
		}

		// (5) Initialize renderer
		if ( !XR_UNQUALIFIED_SUCCESS( InitRender( { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB }, { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT } ) ) )
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
        return EXIT_SUCCESS;
        #endif
	}

	void App::CreateGraphicsPipelines()
	{
		// (1) Vismask
		if ( m_pVisMask )
			CreatePipeline_VisMask( defaultShaders.vismaskVertexShaders, defaultShaders.vismaskFragmentShaders );

		// (2) Pipeline for primitives
		pipelines.primitiveLayout = pRenderInfo->AddNewLayout();
		pipelines.primitives = pRenderInfo->AddNewPipeline();
		CreatePipeline_Primitives( pipelines.primitiveLayout, pipelines.primitives, defaultShaders.primitivesVertexShader, defaultShaders.primitivesFragmentShader );

		// (3) Main pipeline for pbr
		m_pRender->CreateGraphicsPipeline_PBR(
        #ifdef XR_USE_PLATFORM_ANDROID
            m_pRender->GetAppInstance()->GetAssetManager(),
        #endif
			pipelines,
			pipelines.pbr,
			pRenderInfo.get(),
			25, // potential mesh count
			mainRenderPass,
			defaultShaders.meshPbrVertexShader,
			defaultShaders.meshPbrFragmentShader,
			true	// Create pbr and scene lighting layout and descriptors
		);

		// (4) Night grid backdrop pipelines

		// The opaque floor writes depth first, then the sky sits at the far plane so the depth test skips
		// every pixel the floor already covers
		SPipelineStateInfo backdropState = m_pRender->CreateDefaultPipelineState(
			m_pRender->GetTextureWidth(),
			m_pRender->GetTextureHeight() );
		backdropState.rasterization.cullMode = VK_CULL_MODE_NONE;

		m_pRender->CreateGraphicsPipeline_CustomPBR(
        #ifdef XR_USE_PLATFORM_ANDROID
                m_pRender->GetAppInstance()->GetAssetManager(),
        #endif
			pipelines,
			pipelines.floor,
			pRenderInfo.get(),
			mainRenderPass,
			"backdrop.vert.spv",
			"night_grid_floor.frag.spv",
			backdropState,
			false, // reuse main pbr pipeline layouts
			4	   // potential mesh count
		);

		backdropState.depthStencil.depthWriteEnable = VK_FALSE;
		backdropState.depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		m_pRender->CreateGraphicsPipeline_CustomPBR(
        #ifdef XR_USE_PLATFORM_ANDROID
                m_pRender->GetAppInstance()->GetAssetManager(),
        #endif
			pipelines,
			pipelines.sky,
			pRenderInfo.get(),
			mainRenderPass,
			"backdrop_far.vert.spv",
			"night_grid_sky.frag.spv",
			backdropState,
			false,
			4
		);

		// Stars and shooting stars add their glow over the sky, still behind anything that wrote depth
		auto &starBlend = backdropState.colorBlendAttachments.front();
		starBlend.blendEnable = VK_TRUE;
		starBlend.srcColorBlendFactor = starBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		starBlend.colorBlendOp = VK_BLEND_OP_ADD;
		starBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		starBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		starBlend.alphaBlendOp = VK_BLEND_OP_ADD;

		m_pRender->CreateGraphicsPipeline_CustomPBR(
        #ifdef XR_USE_PLATFORM_ANDROID
                m_pRender->GetAppInstance()->GetAssetManager(),
        #endif
			pipelines,
			m_unStarsPipeline,
			pRenderInfo.get(),
			mainRenderPass,
			"night_grid_stars.vert.spv",
			"night_grid_stars.frag.spv",
			backdropState,
			false,
			4
		);
	}

	void App::SetupScene() 
	{
		// (1) Adjust lighting for this demo
		if ( pRenderInfo->pSceneLighting )
		{
			pRenderInfo->pSceneLighting->mainLight.intensity = 1.0f;
			pRenderInfo->pSceneLighting->ambientIntensity = 1.0f;

			pRenderInfo->pSceneLighting->tonemapping.setRenderMode( ERenderMode::Unlit );

			// The night grid is HDR, so map it down rather than clipping
			pRenderInfo->pSceneLighting->tonemapping.setTonemapOperator( ETonemapOperator::KHRNeutral );
		}

		// (2) Night grid lighting, the backdrop shaders also take their brightness from its intensity
		const auto iblBytes = ReadBinaryFile(
		#ifdef XR_USE_PLATFORM_ANDROID
			m_pRender->GetAppInstance()->GetAssetManager(),
		#endif
			"PreparedNightGrid/night_grid.ibl" );

		assets.pNightLighting = std::make_shared< CEnvironmentLighting >();
		const auto environment = DecodeEnvironment( { reinterpret_cast< const uint8_t * >( iblBytes.data() ), iblBytes.size() } );
		if ( assets.pNightLighting->Init( m_pRender->GetLogicalDevice(), m_pRender->GetPhysicalDevice(), m_pRender->GetCommandPool(), m_pXrSession->GetVulkan()->GetVkQueue_Graphics(), environment ) != VK_SUCCESS ||
			 pRenderInfo->SetEnvironment( assets.pNightLighting, 1.f, 0.f ) != VK_SUCCESS )
			LogError( APP_NAME, "Unable to set up the night grid lighting" );

		// (3) Baked night sky cubemap
		const auto skyBytes = ReadBinaryFile(
		#ifdef XR_USE_PLATFORM_ANDROID
			m_pRender->GetAppInstance()->GetAssetManager(),
		#endif
			"PreparedNightGrid/night_grid.sky" );

		auto sky = DecodeBackground( { reinterpret_cast< const uint8_t * >( skyBytes.data() ), skyBytes.size() } );
		if ( pTextureManager->CreateCubeTextureFromData( assets.skyTexture, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, sky.pixels.data(), sky.size ) != VK_SUCCESS )
			LogError( APP_NAME, "Unable to upload the night sky" );

		// (4) Backdrop meshes, generated rather than loaded
		uint32_t unBackdropPool = 0;
		pRenderInfo->pDescriptors->CreateDescriptorPool( unBackdropPool, pipelines.pbrFragmentDescriptorLayout, 4 );

		assets.pSky = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.sky );
		assets.pFloor = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.floor );
		assets.pStars = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, m_unStarsPipeline );
		assets.pShootingStar = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, m_unStarsPipeline );

		assets.pSky->textures = { assets.skyTexture };
		SBackdrop::InitSky( *assets.pSky, 0 );
		SBackdrop::InitFloor( *assets.pFloor );
		SBackdrop::InitStars( *assets.pStars );
		m_shootingStar.Init( *assets.pShootingStar, 0 );

		// (5) Load materials, the sky's carries the cubemap
		if ( assets.pSky->LoadMaterial( gamestate.vecMaterialData, pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, unBackdropPool, pTextureManager.get() ) != 1 ||
			 assets.pFloor->LoadMaterial( pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, unBackdropPool, pTextureManager.get() ) != 1 ||
			 assets.pStars->LoadMaterial( pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, unBackdropPool, pTextureManager.get() ) != 1 ||
			 assets.pShootingStar->LoadMaterial( pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, unBackdropPool, pTextureManager.get() ) != 1 )
			LogError( APP_NAME, "Unable to set up the backdrop materials" );

		// (6) Background keeps list order, so the floor's depth is in place before the sky, then stars blend over it
		for ( auto pBackdrop : { assets.pFloor, assets.pSky, assets.pStars, assets.pShootingStar } )
		{
			pBackdrop->renderQueue = ERenderQueue::Background;
			pBackdrop->InitBuffers();
			pRenderInfo->vecRenderables.push_back( pBackdrop );
		}

		assets.pShootingStar->isVisible = false;
		m_backdropStarted = std::chrono::steady_clock::now();
	}

	void App::UpdateBackdrop()
	{
		if ( !assets.pSky )
			return;

		XrSpaceLocation head { XR_TYPE_SPACE_LOCATION };
		if ( XR_FAILED( xrLocateSpace( m_pXrSession->GetHmdSpace(), m_pXrSession->GetAppSpace(), pRenderInfo->state.frameState.predictedDisplayTime, &head ) ) )
			return;

		// Keep the sky centred on the head so it reads as distant
		if ( head.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT )
			assets.pSky->instances[ 0 ].pose.position = assets.pStars->instances[ 0 ].pose.position = assets.pShootingStar->instances[ 0 ].pose.position = head.pose.position;

		const float seconds = std::chrono::duration< float >( std::chrono::steady_clock::now() - m_backdropStarted ).count();
		assets.pShootingStar->isVisible = m_shootingStar.Update( assets.pShootingStar->vertices, seconds, head.pose.orientation );
		if ( assets.pShootingStar->isVisible )
			assets.pShootingStar->UpdateVertexBuffer();
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
				// Add alpha to clear color to allow the projection layer to show through a bit
				pRenderInfo->state.compositionLayerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
				pRenderInfo->state.clearValues[ 0 ].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

				if( pRenderInfo->state.postAppFrameLayers.empty() )
					GetPassthrough()->GetCompositionLayers( pRenderInfo->state.postAppFrameLayers, false );
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

	void App::ActionCallback_SetControllerActive( SAction *pAction, uint32_t unActionStateIndex )
	{
		if ( unActionStateIndex == 0 )
			gamestate.bLeftControllerActive = pAction->vecActionStates[ 0 ].statePose.isActive;
		else if ( unActionStateIndex == 1 )
			gamestate.bRightControllerActive = pAction->vecActionStates[ 1 ].statePose.isActive;
	}

	void App::ActionCallback_Pinch( SAction *pAction, uint32_t unActionStateIndex )
	{
		if ( unActionStateIndex == 0 )
			gamestate.leftPinchStrength = pAction->vecActionStates[ 0 ].stateFloat.currentState;
		else if ( unActionStateIndex == 1 )
			gamestate.rightPinchStrength = pAction->vecActionStates[ 1 ].stateFloat.currentState;
	}

	void App::ActionCallback_Grasp( SAction *pAction, uint32_t unActionStateIndex )
	{
		if( unActionStateIndex == 0 )
			gamestate.leftGraspStrength = pAction->vecActionStates[ 0 ].stateFloat.currentState;
		else if ( unActionStateIndex == 1 )
			gamestate.rightGraspStrength = pAction->vecActionStates[ 1 ].stateFloat.currentState;
	}

	void App::ActionHaptic( SAction *pAction, uint32_t unActionStateIndex ) const
	{
		if ( !pInput )
			return;

		// Haptic params (these are also params of the GenerateHaptic() function
		// adding here for visibility
		uint64_t unDuration = XR_MIN_HAPTIC_DURATION;
		float fAmplitude = 0.5f;
		float fFrequency = XR_FREQUENCY_UNSPECIFIED;

		// Check if the action has subpaths
		if ( !pAction->vecSubactionpaths.empty() )
		{
			pInput->GenerateHaptic( pAction->xrActionHandle, pAction->vecSubactionpaths[ unActionStateIndex ], unDuration, fAmplitude, fFrequency );
		}
		else
		{
			pInput->GenerateHaptic( pAction->xrActionHandle, XR_NULL_PATH, unDuration, fAmplitude, fFrequency );
		}
	}

	void App::ActionCallback_NoAction( SAction *pAction, uint32_t unActionStateIndex )
	{
		// Placeholder for no action callback (i.e. no action is triggered)
	}

} // namespace app
