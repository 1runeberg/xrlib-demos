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
#include <tinygltf/tiny_gltf.h>

namespace xrapp
{

#ifdef XR_USE_PLATFORM_ANDROID

	XrApp::XrApp( struct android_app *pAndroidApp,const std::string &sAppName, const XrVersion32 unAppVersion, const ELogLevel eMinLogLevel )
	{
		// Create thread pool
		pThreadPool = std::make_unique< CThreadPool >( pAndroidApp->activity->vm ); // Will use optimal number of worker threads - starts with 1 and dynamically increases to max as needed

		// Create an xr instance, we'll leave the optional log level parameter to verbose.
		m_pXrInstance = std::make_unique< CInstance >( pAndroidApp, sAppName, unAppVersion, eMinLogLevel );

		// Initialize android openxr loader
		if ( !XR_UNQUALIFIED_SUCCESS( m_pXrInstance->InitAndroidLoader( pAndroidApp ) ) )
			throw;
	}
#else
	XrApp::XrApp( int argc, char *argv[], const std::string &sAppName, const XrVersion32 unAppVersion, const ELogLevel eMinLogLevel )
	{
		// Create thread pool
		pThreadPool = std::make_unique< CThreadPool >(); // Will use optimal number of worker threads - starts with 2 and dynamically increases to max as needed

		// Create an xr instance, we'll leave the optional log level parameter to verbose.
		m_pXrInstance = std::make_unique< CInstance >( sAppName, unAppVersion, eMinLogLevel );
	}
#endif

	XrApp::~XrApp() {}

	XrResult XrApp::InitInstance( std::vector< const char * > &vecExtensions, std::vector< const char * > &vecApiLayers, const XrInstanceCreateFlags createFlags, const void *pNext )
	{
		// Filter out requested extensions and api layers
		XR_RETURN_ON_ERROR( m_pXrInstance->RemoveUnsupportedExtensions( vecExtensions ) );
		XR_RETURN_ON_ERROR( m_pXrInstance->RemoveUnsupportedApiLayers( vecApiLayers ) );

		// Initialize OpenXR instance
		XR_RETURN_ON_ERROR( m_pXrInstance->Init( vecExtensions, vecApiLayers, createFlags, pNext ) );

		return XR_SUCCESS;
	}

	XrResult XrApp::InitSession( SSessionSettings &settings )
	{
		if ( m_pXrInstance.get() == nullptr || m_pXrInstance->GetXrInstance() == XR_NULL_HANDLE )
			return XR_ERROR_CALL_ORDER_INVALID;

		// Initialize session
		m_pXrSession = std::make_unique< CSession >( m_pXrInstance.get() );
		XR_RETURN_ON_ERROR( m_pXrSession->Init( settings ) );

		// Setup supported extensions
		if ( m_pXrInstance->IsExtensionEnabled( XR_KHR_VISIBILITY_MASK_EXTENSION_NAME ) )
			m_pVisMask = std::make_unique< KHR::CVisibilityMask >( m_pXrInstance->GetXrInstance() );

		if ( m_pXrInstance->IsExtensionEnabled( XR_EXT_HAND_TRACKING_EXTENSION_NAME ) )
			m_pHandTracking = std::make_unique< EXT::CHandTracking >( m_pXrInstance->GetXrInstance() );

		if ( m_pXrInstance->IsExtensionEnabled( XR_FB_PASSTHROUGH_EXTENSION_NAME ) )
		{
			m_pPassthrough = std::make_unique< FB::CPassthrough >( m_pXrInstance->GetXrInstance() );
		}

		if ( m_pXrInstance->IsExtensionEnabled( XR_FB_TRIANGLE_MESH_EXTENSION_NAME ) )
		{
			m_pTriangleMesh = std::make_unique< FB::CTriangleMesh >( m_pXrInstance->GetXrInstance() );
		}

		if ( m_pXrInstance->IsExtensionEnabled( XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME ) )
		{
			m_pDisplayRate = std::make_unique< FB::CDisplayRefreshRate >( m_pXrInstance->GetXrInstance() );

			if ( m_pDisplayRate )
			{
				XrSession xrSession = m_pXrSession->GetXrSession();
				std::vector< float > supportedRates;
				m_pDisplayRate->GetSupportedRefreshRates( m_pXrSession->GetXrSession(), supportedRates );

				// Look for highest rate
				float maxRate = *std::max_element( supportedRates.begin(), supportedRates.end() );
				m_pDisplayRate->RequestRefreshRate( xrSession, maxRate );

				LogInfo( m_pXrInstance->GetAppName(), "Requested refresh rate: %f", maxRate );
				LogInfo( m_pXrInstance->GetAppName(), "Current refresh rate: %f", m_pDisplayRate->GetCurrentRefreshRate( xrSession ) );
			}
		}
		return XR_SUCCESS;
	}

	XrResult XrApp::InitRender( const std::vector< int64_t > &vecColorFormats, const std::vector< int64_t > &vecDepthFormats, const uint32_t unTextureFaceCount, const uint32_t unTextureMipCount )
	{
		VkFormat vkFormatColor = (VkFormat) m_pXrSession->SelectColorTextureFormat( vecColorFormats );
		VkFormat vkFormatDepth = (VkFormat) m_pXrSession->SelectDepthTextureFormat( vecDepthFormats );

		if ( vkFormatColor == 0 || vkFormatDepth == 0 )
			return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;

		m_pRender = std::make_unique< CStereoRender >( m_pXrSession.get(), vkFormatColor, vkFormatDepth );
		XR_RETURN_ON_ERROR( m_pRender->Init( unTextureFaceCount, unTextureMipCount ) );

		// Create render info
		pRenderInfo = std::make_unique< CRenderInfo >( m_pXrSession.get() );

		// Create texture manager
		pTextureManager = std::make_unique< CTextureManager >( m_pXrSession.get(), m_pRender->GetCommandPool() ); 

		return XR_SUCCESS;
	}

	XrResult XrApp::InitHandTracking( XrHandJointSetEXT leftHandJointSet, XrHandJointSetEXT rightHandJointSet )
	{
		if ( !m_pHandTracking )
			return XR_ERROR_FEATURE_UNSUPPORTED;

		return InitHandTracking( leftHandJointSet, nullptr, rightHandJointSet, nullptr );
	}

	XrResult XrApp::InitHandTracking( void *pNextLeft, void *pNextRight )
	{
		if ( !m_pHandTracking )
			return XR_ERROR_FEATURE_UNSUPPORTED;

		return InitHandTracking( XR_HAND_JOINT_SET_DEFAULT_EXT, pNextLeft, XR_HAND_JOINT_SET_DEFAULT_EXT, pNextRight );
	}

	XrResult XrApp::InitHandTracking( XrHandJointSetEXT leftHandJointSet,
									  void *pNextLeft,
									  XrHandJointSetEXT rightHandJointSet,
									  void *pNextRight )
	{
		if ( !m_pHandTracking )
			return XR_ERROR_FEATURE_UNSUPPORTED;

		return m_pHandTracking->Init( m_pXrSession->GetXrSession(), leftHandJointSet, pNextLeft, rightHandJointSet, pNextRight );
	}


	XrResult XrApp::InitPassthrough( void *pOtherInfo )
	{ 
		if ( !m_pPassthrough )
			return XR_ERROR_FEATURE_UNSUPPORTED;

		return m_pPassthrough->Init( m_pXrSession->GetXrSession(), m_pXrInstance.get(), pOtherInfo );
	}

	XrResult XrApp::CreateMainRenderPass() 
	{ 
		return m_pRender->CreateRenderPass( mainRenderPass, ( GetVisMask() != nullptr ) ); 
	}

	VkResult XrApp::CreatePipeline_VisMask( const std::vector< std::string > &vecVertexShaders, const std::vector< std::string > &vecFragmentShaders )
	{
		if ( !BUseVisVMask() )
			return VK_SUCCESS;

		return m_pRender->CreateGraphicsPipeline_Stencils(
			#ifdef XR_USE_PLATFORM_ANDROID
				m_pXrInstance->GetAndroidApp()->activity->assetManager,
			#endif
			pRenderInfo->stencilLayout,
			pRenderInfo->stencilPipelines,		
			mainRenderPass,
			vecVertexShaders,
			vecFragmentShaders );
	}

	VkResult XrApp::CreatePipeline_Primitives( const uint32_t layoutIndex, const uint32_t pipelineIndex, const std::string &sVertexShader, const std::string &sFragmentShader )
	{
		return m_pRender->CreateGraphicsPipeline_Primitives(
		#ifdef XR_USE_PLATFORM_ANDROID
			m_pXrInstance->GetAndroidApp()->activity->assetManager,
		#endif
			pRenderInfo->vecPipelineLayouts[ layoutIndex ],
			pRenderInfo->vecGraphicsPipelines[ pipelineIndex ],
			mainRenderPass,
			sVertexShader,
			sFragmentShader ); 
	}

	VkResult XrApp::CreatePipeline( const uint32_t layoutIndex, const uint32_t pipelineIndex, std::vector< VkDescriptorSetLayout > &vecLayouts, SShaderSet *pShaderSet_WillBeDestroyed ) 
	{ 
		return m_pRender->CreateGraphicsPipeline(
			pRenderInfo->vecPipelineLayouts[ layoutIndex ],
			pRenderInfo->vecGraphicsPipelines[ pipelineIndex ],
			mainRenderPass,
			vecLayouts,
			pShaderSet_WillBeDestroyed ); 
	}

	void XrApp::CreateVismasks()
	{
		if ( m_pVisMask == nullptr )
			return;

		// Retrieve left eye vismask and convert to a Plane2D
		vecMasks.push_back( new CPlane2D( m_pXrSession.get(), pRenderInfo.get(), 0, 0 ) );
		m_pVisMask->GetVisMaskShortIndices(
			m_pXrSession->GetXrSession(),
			*vecMasks.back()->GetVertices(),
			*vecMasks.back()->GetIndices(),
			XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
			k_Left,
			XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
		vecMasks.back()->InitBuffers();

		// Retrieve right eye vismask and convert to a Plane2D
		vecMasks.push_back( new CPlane2D( m_pXrSession.get(), pRenderInfo.get(), 0, 0 ) );
		m_pVisMask->GetVisMaskShortIndices(
			m_pXrSession->GetXrSession(),
			*vecMasks.back()->GetVertices(),
			*vecMasks.back()->GetIndices(),
			XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
			k_Right,
			XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
		vecMasks.back()->InitBuffers();		
	}

	void XrApp::ProcessEvents_SessionState( XrEventDataBaseHeader &xrEventDataBaseheader ) 
	{
		if ( xrEventDataBaseheader.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED )
		{
			if ( m_pXrSession->GetState() == XR_SESSION_STATE_READY )
			{
				// Start session - begin the app's frame loop here
				if ( !XR_UNQUALIFIED_SUCCESS( m_pXrSession->Start() ) )
					return;

				m_bInputActive = false;
				LogInfo( m_pXrInstance->GetAppName(), "OpenXR session started." );
			}
			else if ( m_pXrSession->GetState() == XR_SESSION_STATE_STOPPING )
			{
				// End session - end the app's frame loop here
				if ( !XR_UNQUALIFIED_SUCCESS( m_pXrSession->End() ) )
					return;

				m_bInputActive = false;
				LogInfo( m_pXrInstance->GetAppName(), "OpenXR session exiting." );
			}
			else if ( m_pXrSession->GetState() == XR_SESSION_STATE_FOCUSED )
			{
				m_bInputActive = true;
				LogInfo( m_pXrInstance->GetAppName(), "Input now active." );
			}
			else if ( m_pXrSession->GetState() < XR_SESSION_STATE_FOCUSED )
			{
				m_bInputActive = false;
			}
		} 
	}

	bool XrApp::ProcessEvents_Vismasks( XrEventDataBaseHeader &xrEventDataBaseheader ) 
	{ 
		if ( xrEventDataBaseheader.type == XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR && GetVisMask() && vecMasks.size() == 2 )
		{
			GetVisMask()->UpdateVisMaskShortIndices(
				xrEventDataBaseheader,
				GetSession()->GetXrSession(),
				*vecMasks.at( 0 )->GetVertices(),
				*vecMasks.at( 0 )->GetIndices(),
				XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
				k_Left,
				XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
			vecMasks.at( 0 )->InitBuffers();

			GetVisMask()->UpdateVisMaskShortIndices(
				xrEventDataBaseheader,
				GetSession()->GetXrSession(),
				*vecMasks.at( 1 )->GetVertices(),
				*vecMasks.at( 1 )->GetIndices(),
				XrViewConfigurationType::XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
				k_Right,
				XrVisibilityMaskTypeKHR::XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR );
			vecMasks.at( 1 )->InitBuffers();

			return true;
		}

		return false;
	}

	void XrApp::ProcessEvents_DisplayRateChanged( XrEventDataBaseHeader &xrEventDataBaseheader ) 
	{ 
		if ( !m_pDisplayRate )
			return;

		if ( xrEventDataBaseheader.type == XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB )
		{
			LogInfo( m_pXrInstance->GetAppName(), "Refresh rate changed to: %f", m_pDisplayRate->GetCurrentRefreshRate( m_pXrSession->GetXrSession() ) );
		}

	}

	void XrApp::ActionCallback_Debug( SAction *pAction, uint32_t unActionStateIndex ) 
	{ 
		if ( pAction->vecActionStates.empty() )
		{
			LogDebug( m_pXrInstance->GetAppName(), "Action %" PRIu64 " called succesfully. But action states were empty.", pAction->xrActionHandle );
			return;
		}

		std::string sSubpath;
		if ( pAction->vecSubactionpaths.size() > unActionStateIndex )
		{
			uint32_t unCount = 0;
			char sPath[ XR_MAX_PATH_LENGTH ];
			XrResult xrResult = xrPathToString( m_pXrInstance->GetXrInstance(), pAction->vecSubactionpaths[ unActionStateIndex ], sizeof( sPath ), &unCount, sPath );
			if ( XR_UNQUALIFIED_SUCCESS( xrResult ) )
				sSubpath = sPath;
		}
			
		switch ( pAction->xrActionType )
		{
			case XR_ACTION_TYPE_BOOLEAN_INPUT:
			{
				XrActionStateBoolean &stateB = pAction->vecActionStates[ unActionStateIndex ].stateBoolean;
				if ( stateB.isActive )
				{
					if ( stateB.changedSinceLastSync )
						LogDebug( m_pXrInstance->GetAppName(), "Action %" PRIu64 " [%s] called succesfully. Current state is: %s", 
							pAction->xrActionHandle, 
							sSubpath.c_str(),
							stateB.currentState ? "true" : "false" );
				}
				return;
				break;
			}

			case XR_ACTION_TYPE_FLOAT_INPUT:
			{
				XrActionStateFloat &stateF = pAction->vecActionStates[ unActionStateIndex ].stateFloat;
				if ( stateF.isActive )
				{
					if ( stateF.changedSinceLastSync )
						LogDebug( m_pXrInstance->GetAppName(), 
							"Action %" PRIu64 " [%s] called succesfully. Current state is: %f", 
							pAction->xrActionHandle, 
							sSubpath.c_str(),
							stateF.currentState );
				}
				return;
				break;
			}

			case XR_ACTION_TYPE_VECTOR2F_INPUT:
			{
				XrActionStateVector2f &stateV2 = pAction->vecActionStates[ unActionStateIndex ].stateVector2f;
				if ( stateV2.isActive )
				{
					if ( stateV2.changedSinceLastSync )
						LogDebug( m_pXrInstance->GetAppName(), "Action %" PRIu64 " [%s] called succesfully. Current state is: { %f, %f }", 
							pAction->xrActionHandle, 
							sSubpath.c_str(),
							stateV2.currentState.x, 
							stateV2.currentState.y );
				}
				return;
				break;
			}

			case XR_ACTION_TYPE_POSE_INPUT:
			{
				XrActionStatePose &stateP = pAction->vecActionStates[ unActionStateIndex ].statePose;
				if ( stateP.isActive )
					LogDebug( m_pXrInstance->GetAppName(), "Pose Action %" PRIu64 " [%s] is active.", pAction->xrActionHandle, sSubpath.c_str() );
				return;
				break;
			}

			case XR_ACTION_TYPE_VIBRATION_OUTPUT:
				// This action type won't route to here - trigger with CInput::GenerateHaptic in you app code
				return;

			case XR_ACTION_TYPE_MAX_ENUM:
			default:
				LogError( m_pXrInstance->GetAppName(), "Action %" PRIu64 " [%s] of unknown/unsupported type called.", pAction->xrActionHandle, sSubpath.c_str() );
				break;
		}

	}

	void XrApp::ParallelLoadMeshes( std::vector< SMeshInfo > meshes ) 
	{
		assert( pThreadPool && !meshes.empty() );

		std::unique_ptr< CGltf > pGltf = std::make_unique< CGltf >( m_pXrSession.get() );
		std::vector< std::future< void > > futures;
		std::vector< std::unique_ptr< tinygltf::Model > > models;

		futures.reserve( meshes.size() );
		models.reserve( meshes.size() );

		// Load meshes from disk (use worker threads from thread pool manager)
		auto start = std::chrono::high_resolution_clock::now();
		LogInfo( m_pXrInstance->GetAppName(), "Parallel loading meshes started. Please wait..." );

		for ( auto &mesh : meshes )
		{
			models.push_back( std::make_unique< tinygltf::Model > () );
			tinygltf::Model *currentModel = models.back().get();

			auto future = pThreadPool->SubmitTask(
				[ pGltf = pGltf.get(),
				renderModel = mesh.pRenderModel,
				model = currentModel,
				filename = mesh.sFilename,
				scale = mesh.scale ]()
				{
                    pGltf->LoadFromDisk( renderModel, model, filename, scale );
				} );

			futures.push_back( std::move( future ) );
		}

		// Synchronize mesh load before parsing model
		for ( auto &future : futures )
		{
			future.wait();
		}

		auto end = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast< std::chrono::seconds >( end - start );
		LogInfo( m_pXrInstance->GetAppName(), "All meshes loaded. Time elapsed: %" PRIu64 " seconds", duration.count() );

		// Parse models 
		start = std::chrono::high_resolution_clock::now();
		LogInfo( m_pXrInstance->GetAppName(), "Parsing models. Please wait..." );

		for ( size_t i = 0; i < meshes.size(); i++ )
		{
			pGltf->ParseModel( meshes[ i ].pRenderModel, models[ i ].get(), m_pRender->GetCommandPool() );
		}

		end = std::chrono::high_resolution_clock::now();
		duration = std::chrono::duration_cast< std::chrono::seconds >( end - start );
		LogInfo( m_pXrInstance->GetAppName(), "All models parsed. Time elapsed: %" PRIu64 " seconds", duration.count() );

		#ifdef XR_USE_PLATFORM_ANDROID
		// For android, force garbage collection via thread pool as it takes quite a bit of time in this platform
		std::vector< std::future< void > > cleanupFutures;

		for ( auto &model : models )
		{
			auto cleanupFuture = pThreadPool->SubmitTask(
				[ model = std::move( model ) ]() mutable
				{
					// Explicitly reset the unique_ptr to release resources
					model.reset();
				} );
			cleanupFutures.push_back( std::move( cleanupFuture ) );
		}
		#endif
	}

} // namespace xrapp
