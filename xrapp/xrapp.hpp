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
#include <xrlib/ext/system_properties.hpp>			// Provides system property/hardware inspection capabilities
#include <xrlib/ext/KHR/visibility_mask.hpp> 		// Stencil out portions of the eye textures that will never be visible to the user
#include <xrlib/ext/EXT/hand_tracking.hpp>			// Handle hand tracking
#include <xrlib/ext/FB/passthrough.hpp>				// Handle pass through
#include <xrlib/ext/FB/triangle_mesh.hpp>			// Handle pass through - for projecting passthrough feed to a mesh
#include <xrlib/ext/FB/display_refresh_rate.hpp>	// Handle display refresh rate, useful for consistent anims across devices

// Helper classes
#include <xrlib/thread_pool.hpp>			 // Provides thread pool management. Will need to run on a system with MIN_THREAD_CAP threads
#include <xrvk/render.hpp>					 // Built-in vulkan renderer. Ensure xrlib build includes xrvk when using this.

using namespace xrlib;

namespace xrapp
{
	class XrApp
	{
	  public:

		struct SMeshInfo	
		{
			CRenderModel *pRenderModel = nullptr;
			std::string sFilename;
			XrVector3f scale = { 1.0f, 1.0f, 1.0f };
		};

		#ifdef XR_USE_PLATFORM_ANDROID

			XrApp(
				struct android_app *pAndroidApp, 
				const std::string &sAppName, 
				const XrVersion32 unAppVersion, 
				const ELogLevel eMinLogLevel );
		#else
			XrApp( 
				int argc, 
				char *argv[], 
				const std::string &sAppName, 
				const XrVersion32 unAppVersion, 
				const ELogLevel eMinLogLevel );
		#endif

			~XrApp();

		XrResult InitInstance( 
			std::vector< const char * > &vecExtensions, 
			std::vector< const char * > &vecApiLayers, 
			const XrInstanceCreateFlags createFlags = 0, 
			const void *pNext = nullptr );
	
		XrResult InitSession( SSessionSettings &settings );

		XrResult InitRender( 
			const std::vector< int64_t > &vecColorFormats, 
			const std::vector< int64_t > &vecDepthFormats, 
			const uint32_t unTextureFaceCount = 1, 
			const uint32_t unTextureMipCount = 1 );

		XrResult InitHandTracking( void *pNextLeft, void *pNextRight );

		XrResult InitHandTracking( XrHandJointSetEXT leftHandJointSet, XrHandJointSetEXT rightHandJointSet );

		XrResult InitHandTracking( XrHandJointSetEXT leftHandJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT,
								   void *pNextLeft = nullptr,
								   XrHandJointSetEXT rightHandJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT,
								   void *pNextRight = nullptr );

		XrResult InitPassthrough( void* pOtherInfo = nullptr );

		XrResult CreateMainRenderPass();
		
		VkResult CreatePipeline_VisMask( const std::vector< std::string > &vecVertexShaders, const std::vector< std::string > &vecFragmentShaders );
		VkResult CreatePipeline_Primitives( const uint32_t layoutIndex, const uint32_t pipelineIndex, const std::string &sVertexShader, const std::string &sFragmentShader );
		VkResult CreatePipeline( const uint32_t layoutIndex, const uint32_t pipelineIndex, std::vector< VkDescriptorSetLayout > &vecLayouts, SShaderSet *pShaderSet_WillBeDestroyed );

		void CreateVismasks();

		void ProcessEvents_SessionState( XrEventDataBaseHeader &xrEventDataBaseheader );
		bool ProcessEvents_Vismasks( XrEventDataBaseHeader &xrEventDataBaseheader );
		void ProcessEvents_DisplayRateChanged( XrEventDataBaseHeader &xrEventDataBaseheader );

		void ActionCallback_Debug( SAction *pAction, uint32_t unActionStateIndex );

		void ParallelLoadMeshes( std::vector< SMeshInfo > meshes );


		CInstance *GetInstance() { return m_pXrInstance.get(); }
		CSession *GetSession() { return m_pXrSession.get(); }
		CStereoRender *GetRender() { return m_pRender.get(); }

		KHR::CVisibilityMask *GetVisMask() { return m_pVisMask.get(); }
		bool BUseVisVMask() { return m_pRender && m_pRender->GetUseVisMask(); }

		EXT::CHandTracking *GetHandTracking() { return m_pHandTracking.get(); }
		FB::CPassthrough *GetPassthrough() { return m_pPassthrough.get(); }
		FB::CTriangleMesh *GetTriangleMesh() { return m_pTriangleMesh.get(); }
		FB::CDisplayRefreshRate *GetDisplayRate() { return m_pDisplayRate.get(); }

		std::vector< CPlane2D * > vecMasks;

		struct DefaultShaders
		{
			std::vector< std::string > vismaskVertexShaders = { "stencil0.vert.spv", "stencil1.vert.spv" };
			std::vector< std::string > vismaskFragmentShaders = { "stencil.frag.spv", "stencil.frag.spv" };

			std::string primitivesVertexShader = "primitive.vert.spv";
			std::string primitivesFragmentShader = "primitive.frag.spv";

			std::string meshFlatVertexShader = "mesh_flat.vert.spv";
			std::string meshFlatFragmentShader = "mesh_flat.frag.spv";

			std::string meshPbrVertexShader = "mesh_pbr.vert.spv";
			std::string meshPbrFragmentShader = "mesh_pbr.frag.spv";
			std::string meshPbrFragmentSkyShader = "mesh_pbr_sky.frag.spv";
			std::string meshPbrFragmentFloorShader = "mesh_pbr_floor.frag.spv";

			std::string meshFlatSkeltVertexShader = "mesh_flat_skel.vert.spv";
			std::string meshFlatSkelFragmentShader = "mesh_flat_skel.frag.spv";
		}defaultShaders;

		VkRenderPass mainRenderPass = VK_NULL_HANDLE;

		// Render helpers
		std::unique_ptr< CRenderInfo > pRenderInfo = nullptr;
		std::unique_ptr< CThreadPool > pThreadPool = nullptr;
		std::unique_ptr< CTextureManager > pTextureManager = nullptr;

	  protected:
		bool m_bInputActive = false;

		std::unique_ptr< CInstance > m_pXrInstance = nullptr;
		std::unique_ptr< CSession > m_pXrSession = nullptr;
		std::unique_ptr< CStereoRender > m_pRender = nullptr;

		std::unique_ptr< KHR::CVisibilityMask > m_pVisMask = nullptr;
		std::unique_ptr< EXT::CHandTracking > m_pHandTracking = nullptr;
		std::unique_ptr< FB::CPassthrough > m_pPassthrough = nullptr;
		std::unique_ptr< FB::CTriangleMesh > m_pTriangleMesh = nullptr;
		std::unique_ptr< FB::CDisplayRefreshRate > m_pDisplayRate = nullptr;

	};

} // namespace xrapp