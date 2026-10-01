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

#include <xrlib/ext/EXT/hand_interaction.hpp>
#include <xrlib/ext/EXT/hand_joints_motion_range.hpp>
#include <xrlib/ext/META/simultaneous_hands_and_controllers.hpp>

using namespace xrlib;
using namespace xrapp;

#define APP_NAME "interactionsxr"
namespace app
{
	constexpr XrPosef k_windowPose { { 0.f, 0.f, 0.f, 1.f }, { 0.f, 2.5f, -5.f } };

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

			void ActionCallback_NoAction( SAction *pAction, uint32_t unActionStateIndex );
			void ActionCallback_SetControllerActive( SAction *pAction, uint32_t unActionStateIndex );
			void ActionCallback_Pinch( SAction *pAction, uint32_t unActionStateIndex );
			void ActionCallback_Grasp( SAction *pAction, uint32_t unActionStateIndex );

			void ActionHaptic( SAction *pAction, uint32_t unActionStateIndex ) const;

			// Night grid backdrop, the sky, stars and shooting star follow the head
			struct SAssets
			{
				CRenderModel *pSky = nullptr;
				CRenderModel *pFloor = nullptr;
				CRenderModel *pStars = nullptr;
				CRenderModel *pShootingStar = nullptr;

				STexture skyTexture;
				std::shared_ptr< CEnvironmentLighting > pNightLighting;
			}assets;

			struct SGameState
			{
				ERenderMode currentRenderMode = ERenderMode::Unlit;
				ETonemapOperator currentToneMapper = ETonemapOperator::None;

				std::vector< SMaterialUBO * > vecMaterialData;

				bool bLeftControllerActive = false;
				bool bRightControllerActive = false;

				float leftPinchStrength = 0.0f;
				float rightPinchStrength = 0.0f;

				float leftPokeStrength = 0.0f;
				float rightPokeStrength = 0.0f;

				float leftGraspStrength = 0.0f;
				float rightGraspStrength = 0.0f;

			} gamestate;

			SPipelines pipelines;
			EXT::CHandJointsMotionRange *GetHandsJointsMotionRange() { return m_pHandsJointsMotionRange.get(); }
			META::CHandsAndControllers *GetHandsAndControllers() { return m_pHandsAndControllers.get(); }
			std::unique_ptr< CInput > pInput = nullptr;
			SAction* pHapticAction = nullptr;

	  private:
			void CreateGraphicsPipelines();
			void ReleaseBackdrop();

			uint32_t m_unStarsPipeline = 0;
			SShootingStar m_shootingStar;
			std::chrono::steady_clock::time_point m_backdropStarted = std::chrono::steady_clock::now();

			// Plane facing user for passthrough mesh projection
			std::vector< XrVector3f > m_vecVertices;
			std::vector< uint32_t > m_vecIndices;

			std::unique_ptr < EXT::CHandJointsMotionRange > m_pHandsJointsMotionRange = nullptr;
			std::unique_ptr < META::CHandsAndControllers > m_pHandsAndControllers = nullptr;

	};

} // namespace xrapp