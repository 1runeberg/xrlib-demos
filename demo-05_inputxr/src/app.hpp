/*
 * Copyright 2024-26 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
 */


#include <iostream>
#include <memory>
#include <chrono>

#include <xrapp.hpp>
#include <xrvk/environment.hpp>

#include "backdrop.hpp"
#include "shooting_star.hpp"

using namespace xrlib;
using namespace xrapp;

namespace app
{
	class App : public XrApp
	{
	  public:

		#ifdef XR_USE_PLATFORM_ANDROID

			App( 
				struct android_app *pAndroidApp, 
				const std::string &sAppName, 
				const XrVersion32 unAppVersion, 
				const ELogLevel eMinLogLevel );

			void Init();
		#else
			App( 
				int argc, 
				char *argv[], 
				const std::string &sAppName, 
				const XrVersion32 unAppVersion, 
				const ELogLevel eMinLogLevel );

			int Init();
		#endif

			~App();

			void SetupScene();
			void UpdateBackdrop();
			void ProcessXrEvents( XrEventDataBaseHeader &xrEventDataBaseheader );

			bool StartRenderFrame();
			void EndRenderFrame();

			static bool ScaleBlade( XrVector3f &outScale, float inputValue, float refreshRate, float scaleSpeed = 0.001f );

			void ActionCallback_SetControllerActive( SAction *pAction, uint32_t unActionStateIndex ) const;
			void ActionCallback_ScaleBlade( SAction *pAction, uint32_t unActionStateIndex );
			void ActionCallback_CycleRenderMode( SAction *pAction, uint32_t unActionStateIndex );
			void ActionCallback_TogglePassthrough( SAction *pAction, uint32_t unActionStateIndex );

			void ActionHaptic( SAction *pAction, uint32_t unActionStateIndex ) const;

			struct SAppPipelines : public SPipelines
			{
				uint32_t bladePipeline = 0;
				uint32_t buttonPipeline = 0;
			}pipelines;

			// Night grid backdrop, the sky, stars and shooting star follow the head
			struct SAssets
			{
				CRenderModel *pSky = nullptr;
				CRenderModel *pFloor = nullptr;
				CRenderModel *pStars = nullptr;
				CRenderModel *pShootingStar = nullptr;

				STexture skyTexture;
				std::shared_ptr< CEnvironmentLighting > pNightLighting;
				
				CRenderModel *pHiltLeft = nullptr; 
				CRenderModel *pHiltRight = nullptr; 

				CRenderModel *pBladeLeft = nullptr; 
				CRenderModel *pBladeRight = nullptr; 

				CRenderModel *pButtonBottomLeft = nullptr;
				CRenderModel *pButtonTopLeft = nullptr; 

				CRenderModel *pButtonBottomRight = nullptr;
				CRenderModel *pButtonTopRight = nullptr; 

			}assets;

			struct SPlasmaEffect
			{
				std::chrono::steady_clock::time_point lastFrameTime = std::chrono::steady_clock::now(); 
				float accumulatedTime = 0.0f;
				bool firstFrame = true; 

				void UpdateEffect( SMaterialUBO *left, SMaterialUBO *right )
				{
					auto currentTime = std::chrono::steady_clock::now();

					// Handle first frame
					if ( firstFrame )
					{
						lastFrameTime = currentTime;
						firstFrame = false;
					}

					float deltaTime = std::chrono::duration< float >( currentTime - lastFrameTime ).count();

					// Clamp delta time to prevent huge jumps
					const float maxDeltaTime = 1.0f / 30.0f;
					deltaTime = std::min( deltaTime, maxDeltaTime );

					accumulatedTime += deltaTime;
					lastFrameTime = currentTime;

					// Update material
					if ( left )
						left->emissiveFactor[ 3 ] = accumulatedTime;

					if ( right )
						right->emissiveFactor[ 3 ] = accumulatedTime;

				}
			} plasma;

			struct SGameState
			{
				ERenderMode currentRenderMode = ERenderMode::Unlit;
				ETonemapOperator currentToneMapper = ETonemapOperator::KHRNeutral;

				uint32_t leftBladeMateriaDataId = 0;
				uint32_t rightBladeMateriaDataId = 0;
				std::vector< SMaterialUBO * > vecMaterialData;
			}gamestate;

			std::unique_ptr< CInput > pInput = nullptr;
			SAction* pHapticAction = nullptr;

	  private:
			void CreateGraphicsPipelines();
			void ReleaseBackdrop();

			uint32_t m_unStarsPipeline = 0;
			bool m_bPassthroughView = false;
			SShootingStar m_shootingStar;
			std::chrono::steady_clock::time_point m_backdropStarted = std::chrono::steady_clock::now();

	};

} // namespace xrapp