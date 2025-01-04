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
			XR_FB_PASSTHROUGH_EXTENSION_NAME,	  // Passthrough extension our app will use
			XR_FB_TRIANGLE_MESH_EXTENSION_NAME,	  // Optional, used to project to passthrough to a mesh, not shown in this demo
			XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME // Display refresh rate for consistent animations across devices
			}; 

		// (2) Initialize openxr instance
		if ( !XR_UNQUALIFIED_SUCCESS( InitInstance( vecRequiredExtensions, vecAPILayers ) ) )
			return EXIT_FAILURE;

		// (3) Initialize openxr session
		SSessionSettings defaultSessionSettings;
		if ( !XR_UNQUALIFIED_SUCCESS( InitSession( defaultSessionSettings ) ) )
			return EXIT_FAILURE;

		// ()

		// (5) Initialize FB Passthrough
		if ( GetPassthrough() )
		{
			// (5.1) Initialize passthrough
			if ( !XR_UNQUALIFIED_SUCCESS( InitPassthrough() ) )
				return EXIT_FAILURE;

			// (5.2) Create passthrough layer
			if ( !XR_UNQUALIFIED_SUCCESS(
					 GetPassthrough()->AddLayer( GetSession()->GetXrSession(), ExtBase_Passthrough::ELayerType::FULLSCREEN, XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT, XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB ) ) )
				return EXIT_FAILURE;
		}

		// (6) Initialize renderer
		if ( !XR_UNQUALIFIED_SUCCESS( InitRender( { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB }, { VK_FORMAT_D24_UNORM_S8_UINT } ) ) )
			return EXIT_FAILURE;

		// (7) Create render pass
		if ( !XR_UNQUALIFIED_SUCCESS( CreateMainRenderPass() ) )
			return EXIT_FAILURE;

		// (8) Create graphics pipelines
		CreateGraphicsPipelines();

		// (9) Create vis masks
		CreateVismasks();

		return EXIT_SUCCESS; 
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

		// (4) Sky and Floor pipelines

		// Customize - enable alpha blending
		SPipelineStateInfo pipelineInfo = m_pRender->CreateDefaultPipelineState( 
			m_pRender->GetTextureWidth(), 
			m_pRender->GetTextureHeight() );

		pipelineInfo.colorBlendAttachments.clear();
		VkPipelineColorBlendAttachmentState colorBlending = m_pRender->GenerateColorBlendAttachment( true );
		pipelineInfo.colorBlendAttachments.push_back( colorBlending );
		pipelineInfo.colorBlend = m_pRender->GeneratePipelineStateCI_ColorBlendCI( pipelineInfo.colorBlendAttachments );

		m_pRender->CreateGraphicsPipeline_CustomPBR(
        #ifdef XR_USE_PLATFORM_ANDROID
                m_pRender->GetAppInstance()->GetAssetManager(),
        #endif
			pipelines,
			pipelines.sky,
			pRenderInfo.get(),
			mainRenderPass,
			defaultShaders.meshPbrVertexShader,
			defaultShaders.meshPbrFragmentSkyShader,
			pipelineInfo,
			false, // reuse main pbr pipeline layouts
			2	   // potential mesh count
		);

		m_pRender->CreateGraphicsPipeline_CustomPBR(
        #ifdef XR_USE_PLATFORM_ANDROID
                m_pRender->GetAppInstance()->GetAssetManager(),
        #endif
			pipelines,
			pipelines.floor,
			pRenderInfo.get(),
			mainRenderPass,
			defaultShaders.meshPbrVertexShader,
			defaultShaders.meshPbrFragmentFloorShader, 
			pipelineInfo,
			false, // reuse main pbr pipeline layouts
			2	   // potential mesh count
		);

		// (5) Plasma blade pipeline (based on PBR layout)
		
		m_pRender->CreateGraphicsPipeline_CustomPBR(
        #ifdef XR_USE_PLATFORM_ANDROID
                m_pRender->GetAppInstance()->GetAssetManager(),
        #endif
			pipelines,
			pipelines.bladePipeline,
			pRenderInfo.get(),
			mainRenderPass,
			defaultShaders.meshPbrVertexShader,
			"plasma_blade.frag.spv",
			pipelineInfo,
			false, // reuse main pbr pipeline layouts
			3	   // potential mesh count
		);

		// (6) Plasma blade button pipeline (based on PBR layout)
		m_pRender->CreateGraphicsPipeline_CustomPBR(
        #ifdef XR_USE_PLATFORM_ANDROID
                m_pRender->GetAppInstance()->GetAssetManager(),
        #endif
			pipelines,
			pipelines.buttonPipeline,
			pRenderInfo.get(),
			mainRenderPass,
			defaultShaders.meshPbrVertexShader,
			"plasma_button.frag.spv",
			pipelineInfo,
			false, // reuse main pbr pipeline layouts
			5	   // potential mesh count
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
			pRenderInfo->pSceneLighting->tonemapping.setTonemapOperator( ETonemapOperator::None );
		}
	
		// RENDER MODELS (organized by depth, parallel processing using worker threads from thread pool manager)
		
		// (2) Define models to be used in this app
		assets.pSky = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.sky );
		assets.pFloor = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.floor );
		assets.pHiltLeft = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.pbr );
		assets.pHiltRight = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.pbr );
		assets.pBladeLeft = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.bladePipeline );
		assets.pBladeRight = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.bladePipeline );
		assets.pButtonBottomLeft = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.buttonPipeline );
		assets.pButtonTopLeft = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.buttonPipeline );
		assets.pButtonBottomRight = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.buttonPipeline );
		assets.pButtonTopRight = new CRenderModel( m_pXrSession.get(), pRenderInfo.get(), pipelines.pbrLayout, pipelines.buttonPipeline );

		// Set visibility for buttons
		assets.pButtonBottomLeft->isVisible = false;
		assets.pButtonTopLeft->isVisible = false;
		assets.pButtonBottomRight->isVisible = false;
		assets.pButtonTopRight->isVisible = false;

		// Lift and flip plane for sky 
		assets.pSky->instances[ 0 ].pose.position.y = 100.f;
		assets.pSky->instances[ 0 ].pose.orientation = { 1.0f, 0.0f, 0.0f, 0.0f };

		// (3) Parallel load meshes using built-in thread pool manager
		ParallelLoadMeshes( { 
			{ .pRenderModel = assets.pSky, .sFilename = "plane.glb", .scale = { 5000.0f, 1.f, 5000.0f } },
			{ .pRenderModel = assets.pFloor, .sFilename = "plane.glb", .scale = { 5.0f, 1.f, 5.0f } },
			{ .pRenderModel = assets.pHiltLeft, .sFilename = "Saber/hilt.glb", .scale = { 0.04f, 0.04f, 0.04f } },
			{ .pRenderModel = assets.pHiltRight, .sFilename = "Saber/hilt.glb", .scale = { 0.04f, 0.04f, 0.04f } },
			{ .pRenderModel = assets.pBladeLeft, .sFilename = "Saber/blade.glb", .scale = { 0.04f, 0.04f, 0.04f } },
			{ .pRenderModel = assets.pBladeRight, .sFilename = "Saber/blade.glb", .scale = { 0.04f, 0.04f, 0.04f } },
			{ .pRenderModel = assets.pButtonBottomLeft, .sFilename = "Saber/btnbottom.glb", .scale = { 0.04f, 0.04f, 0.04f } },
			{ .pRenderModel = assets.pButtonTopLeft, .sFilename = "Saber/btntop.glb", .scale = { 0.04f, 0.04f, 0.04f } },
			{ .pRenderModel = assets.pButtonBottomRight, .sFilename = "Saber/btnbottom.glb", .scale = { 0.04f, 0.04f, 0.04f } },
			{ .pRenderModel = assets.pButtonTopRight, .sFilename = "Saber/btntop.glb", .scale = { 0.04f, 0.04f, 0.04f } },
		} );

		// (4) Define and load materials for each mesh

		// load materials for meshes where we don't need to update any material variables (scene lighting is always dynamic)
		assets.pHiltLeft->LoadMaterial( pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, pipelines.pbrFragmentDescriptorPool, pTextureManager.get() );
		assets.pHiltRight->LoadMaterial( pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, pipelines.pbrFragmentDescriptorPool, pTextureManager.get() );
		assets.pButtonBottomLeft->LoadMaterial( pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, pipelines.pbrFragmentDescriptorPool, pTextureManager.get() );
		assets.pButtonTopLeft->LoadMaterial( pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, pipelines.pbrFragmentDescriptorPool, pTextureManager.get() );
		assets.pButtonBottomRight->LoadMaterial( pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, pipelines.pbrFragmentDescriptorPool, pTextureManager.get() );
		assets.pButtonTopRight->LoadMaterial( pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, pipelines.pbrFragmentDescriptorPool, pTextureManager.get() );

		// load materials for meshes where we want dynamic updates to maerial variables
		assets.pSky->LoadMaterial( gamestate.vecMaterialData, pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, pipelines.pbrFragmentDescriptorPool, pTextureManager.get() );
		gamestate.skyMateriaDataId = gamestate.vecMaterialData.size() - 1;
		gamestate.vecMaterialData[ gamestate.skyMateriaDataId ]->emissiveFactor[ 1 ] = 1.0f; // reset opacity value input for shader

		assets.pFloor->LoadMaterial( gamestate.vecMaterialData, pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, pipelines.pbrFragmentDescriptorPool, pTextureManager.get() );
		gamestate.floorMateriaDataId = gamestate.vecMaterialData.size() - 1;

		assets.pBladeLeft->LoadMaterial( gamestate.vecMaterialData, pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, pipelines.pbrFragmentDescriptorPool, pTextureManager.get() );
		gamestate.leftBladeMateriaDataId = gamestate.vecMaterialData.size() - 1;

		assets.pBladeRight->LoadMaterial( gamestate.vecMaterialData, pRenderInfo.get(), pipelines.pbrFragmentDescriptorLayout, pipelines.pbrFragmentDescriptorPool, pTextureManager.get() );
		gamestate.rightBladeMateriaDataId = gamestate.vecMaterialData.size() - 1;

		// (5) Init mesh buffers
		assets.pSky->InitBuffers();
		assets.pFloor->InitBuffers();
		assets.pHiltLeft->InitBuffers();
		assets.pHiltRight->InitBuffers();
		assets.pBladeLeft->InitBuffers();
		assets.pBladeRight->InitBuffers();
		assets.pButtonBottomLeft->InitBuffers();
		assets.pButtonTopLeft->InitBuffers();
		assets.pButtonBottomRight->InitBuffers();
		assets.pButtonTopRight->InitBuffers();

		// (6) Add all meshes to render info for rendering (arranged sequentially as per desired depth draw)
		pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( assets.pSky ) );
		pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( assets.pFloor ) );
		pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( assets.pHiltLeft ) );
		pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( assets.pHiltRight ) );
		pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( assets.pBladeLeft ) );
		pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( assets.pBladeRight ) );
		pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( assets.pButtonBottomLeft ) );
		pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( assets.pButtonTopLeft ) );
		pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( assets.pButtonBottomRight ) );
		pRenderInfo->vecRenderables.push_back( dynamic_cast< CRenderable * >( assets.pButtonTopRight ) );
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

	bool App::ScaleBlade( XrVector3f &outScale, float inputValue, float refreshRate, float scaleSpeed ) 
	{
		if ( refreshRate <= 0.0f )
			return false;

		// Set scale parameters
		const float k_minScale = 0.005f;
		const float k_maxScale = 0.04f;
		const float k_deadzone = 0.4f;		// Center deadzone threshold
		const float k_deadzoneRange = 0.1f; // Size of deadzone range

		// Ensure that we always go back to min scale
		if ( inputValue < 0.01f )
		{
			outScale.z = k_minScale;
			return false;
		}

		// Ensure that we always expand to max scale
		if ( inputValue > 0.9f )
		{
			outScale.z = k_maxScale;
			return true;
		}

		// Convert input value to a -1 to 1 range with deadzone
		float scaleFactor = 0.0f;
		if ( inputValue > k_deadzone + k_deadzoneRange )
		{
			// Map 0.5 to 1.0 range to 0 to 1
			scaleFactor = ( inputValue - ( k_deadzone + k_deadzoneRange ) ) / ( 1.0f - ( k_deadzone + k_deadzoneRange ) );
		}
		else if ( inputValue < k_deadzone - k_deadzoneRange )
		{
			// Map 0.0 to 0.3 range to -1 to 0
			scaleFactor = -1.0f * ( 1.0f - ( inputValue / ( k_deadzone - k_deadzoneRange ) ) );
		}

		// Calculate scale change based on input value
		float fps = 1000.0f / refreshRate;
		float scaleChange = scaleSpeed * fps * scaleFactor;

		// Set scale
		float newScale = outScale.z + scaleChange;

		// Set scale min max limits
		if ( newScale < k_minScale )
			newScale = k_minScale;
		else if ( newScale > k_maxScale )
			newScale = k_maxScale;

		// Update scale
		outScale.z= newScale;

		return true;
	}

	void App::ActionCallback_SetControllerActive( SAction *pAction, uint32_t unActionStateIndex ) 
	{
		bool bState = pAction->vecActionStates[ unActionStateIndex ].statePose.isActive;

		if ( unActionStateIndex == 0 )
		{
			if ( assets.pHiltLeft )
				assets.pHiltLeft->isVisible = bState;

			if ( assets.pBladeLeft )
				assets.pBladeLeft->isVisible = bState;

			return;
		}
		
		if ( unActionStateIndex == 1 )
		{
			if ( assets.pHiltRight )
				assets.pHiltRight->isVisible = bState;

			if ( assets.pBladeRight )
				assets.pBladeRight->isVisible = bState;

			return;
		}
	}

	void App::ActionCallback_ScaleBlade( SAction *pAction, uint32_t unActionStateIndex ) 
	{ 
		XrActionStateFloat state = pAction->vecActionStates[ unActionStateIndex ].stateFloat;
		
		if ( state.isActive )
		{
			XrVector3f &scale = unActionStateIndex == 0 ? assets.pBladeLeft->instances[ 0 ].scale : assets.pBladeRight->instances[ 0 ].scale;

			float displayRate =
				GetDisplayRate() ? GetDisplayRate()->GetCurrentRefreshRate( GetSession()->GetXrSession() ) : 72.0f;
			bool isScaling = ScaleBlade( scale, state.currentState, displayRate );

			if ( isScaling && pHapticAction )
				ActionHaptic( pHapticAction, unActionStateIndex );
		}
	}

	void App::ActionCallback_CycleRenderMode( SAction *pAction, uint32_t unActionStateIndex ) 
	{
		// Update button visibility
		bool &bState = unActionStateIndex == 0 ? assets.pButtonBottomLeft->isVisible : assets.pButtonBottomRight->isVisible;
		bState = pAction->vecActionStates[ unActionStateIndex ].stateBoolean.currentState;
		
		// Cycle through render mode and tone mapping functions
		if ( pAction->vecActionStates[ unActionStateIndex ].stateBoolean.currentState )
		{
			if ( gamestate.currentRenderMode != ERenderMode::PBR )
			{
				switch ( gamestate.currentRenderMode )
				{
					case ERenderMode::Unlit:
						gamestate.currentRenderMode = ERenderMode::BlinnPhong;
						break;
					case ERenderMode::BlinnPhong:
						gamestate.currentRenderMode = ERenderMode::PBR;
						gamestate.currentToneMapper = ETonemapOperator::None;

						pRenderInfo->pSceneLighting->tonemapping.setRenderMode( gamestate.currentRenderMode );
						pRenderInfo->pSceneLighting->tonemapping.setTonemapOperator( gamestate.currentToneMapper );

						return;
						break;
					case ERenderMode::PBR:
						gamestate.currentRenderMode = ERenderMode::Unlit;
						break;
					default:
						gamestate.currentRenderMode = ERenderMode::Unlit;
						break;
				}
			}

			if ( gamestate.currentRenderMode == ERenderMode::PBR )
			{
				switch ( gamestate.currentToneMapper )
				{
					case ETonemapOperator::None:
						gamestate.currentToneMapper = ETonemapOperator::Reinhard;
						break;
					case ETonemapOperator::Reinhard:
						gamestate.currentToneMapper = ETonemapOperator::ACES;
						break;
					case ETonemapOperator::ACES:
						gamestate.currentToneMapper = ETonemapOperator::KHRNeutral;
						break;
					case ETonemapOperator::KHRNeutral:
						gamestate.currentToneMapper = ETonemapOperator::Uncharted2;
						break;
					case ETonemapOperator::Uncharted2:
						gamestate.currentToneMapper = ETonemapOperator::None;
						gamestate.currentRenderMode = ERenderMode::Unlit;

						pRenderInfo->pSceneLighting->tonemapping.setRenderMode( gamestate.currentRenderMode );
						pRenderInfo->pSceneLighting->tonemapping.setTonemapOperator( gamestate.currentToneMapper );

						return;
						break;
					default:
						break;
				}
			}

			pRenderInfo->pSceneLighting->tonemapping.setRenderMode( gamestate.currentRenderMode );
			pRenderInfo->pSceneLighting->tonemapping.setTonemapOperator( gamestate.currentToneMapper );
		}
	}

	void App::ActionCallback_TogglePassthrough( SAction *pAction, uint32_t unActionStateIndex ) 
	{ 
		// Update button visibility
		bool &bState = unActionStateIndex == 0 ? assets.pButtonTopLeft->isVisible : assets.pButtonTopRight->isVisible;
		bState = pAction->vecActionStates[ unActionStateIndex ].stateBoolean.currentState;

		// Start/Stop passthrough if available
		if ( bState )
		{	
			// If sky is above, then fade out
			if ( assets.pSky->instances[ 0 ].pose.position.y > ( skyanim.START_Y / 2 ) )
			{
				skyanim.StartAnimation( false, 0.02f );
				if ( GetPassthrough() )
					GetPassthrough()->Start();
			}
			else
			{
				skyanim.StartAnimation( true, 0.03f );
				if ( GetPassthrough() )
					GetPassthrough()->Stop();
			}

		}
	}

	void App::ActionHaptic( SAction *pAction, uint32_t unActionStateIndex ) 
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


} // namespace app
