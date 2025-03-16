/*
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
 */


#include <iostream>
#include <memory>
#include <chrono>

#include <xrapp.hpp>

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

			struct SAssets
			{
				CRenderModel *pSky = nullptr;
				CRenderModel *pFloor = nullptr;
				
				CRenderModel *pHiltLeft = nullptr; 
				CRenderModel *pHiltRight = nullptr; 

				CRenderModel *pBladeLeft = nullptr; 
				CRenderModel *pBladeRight = nullptr; 

				CRenderModel *pButtonBottomLeft = nullptr;
				CRenderModel *pButtonTopLeft = nullptr; 

				CRenderModel *pButtonBottomRight = nullptr;
				CRenderModel *pButtonTopRight = nullptr; 

			}assets;

			struct SkyAnimation
			{
				bool isAnimating = false;
				bool isReversing = false;
				float animationSpeed = 1.0f;

				// Target values
				static constexpr float START_Y = 100.0f;
				static constexpr float END_Y = 10.0f;
				static constexpr float START_OPACITY = 1.0f;
				static constexpr float END_OPACITY = 0.0f;
				static constexpr float EPSILON = 0.001f;

				bool StartAnimation( bool reverse = false, float speed = 1.0f )
				{
					isAnimating = true;
					isReversing = reverse;
					animationSpeed = speed;
					return true;
				}

				void UpdateAnimation( SAssets &assets, XrTime predictedDisplayPeriod, float displayRate, uint32_t skyMaterialDataId, std::vector< SMaterialUBO * > &vecMaterialData )
				{
					if ( !isAnimating )
						return;

					// Convert XrTime (nanoseconds) to seconds
					float deltaTimeSeconds = static_cast< float >( predictedDisplayPeriod ) * 1e-9f;
					float normalizedDelta = deltaTimeSeconds * displayRate * animationSpeed;

					// Get target values based on animation direction
					float targetY = isReversing ? START_Y : END_Y;
					float targetOpacity = isReversing ? START_OPACITY : END_OPACITY;

					// Update position
					float currentY = assets.pSky->instances[ 0 ].pose.position.y;
					float newY = std::lerp( currentY, targetY, normalizedDelta );
					assets.pSky->instances[ 0 ].pose.position.y = newY;

					// Update opacity
					float currentOpacity = vecMaterialData[ skyMaterialDataId ]->emissiveFactor[ 1 ];
					float newOpacity = std::lerp( currentOpacity, targetOpacity, normalizedDelta * 2.0f );
					vecMaterialData[ skyMaterialDataId ]->emissiveFactor[ 1 ] = newOpacity;

					// Check if animation is complete
					if ( std::abs( newY - targetY ) < EPSILON && std::abs( newOpacity - targetOpacity ) < EPSILON )
					{
						isAnimating = false;
					}
				}

			} skyanim;

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
				ETonemapOperator currentToneMapper = ETonemapOperator::None;

				uint32_t skyMateriaDataId = 0;
				uint32_t floorMateriaDataId = 0;
				uint32_t leftBladeMateriaDataId = 0;
				uint32_t rightBladeMateriaDataId = 0;
				std::vector< SMaterialUBO * > vecMaterialData;
			}gamestate;

			std::unique_ptr< CInput > pInput = nullptr;
			SAction* pHapticAction = nullptr;

	  private:
			void CreateGraphicsPipelines();

	};

} // namespace xrapp