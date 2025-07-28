/*
 * Copyright 2024 Rune Berg (http://runeberg.io | https://github.com/1runeberg)
 * Licensed under Apache 2.0 (https://www.apache.org/licenses/LICENSE-2.0)
 * SPDX-License-Identifier: Apache-2.0
 */


#include <iostream>
#include <memory>
#include <chrono>

#include <xrapp.hpp>

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
			void ProcessXrEvents( XrEventDataBaseHeader &xrEventDataBaseheader );

			bool StartRenderFrame();
			void EndRenderFrame();

			void ActionCallback_NoAction( SAction *pAction, uint32_t unActionStateIndex );
			void ActionCallback_SetControllerActive( SAction *pAction, uint32_t unActionStateIndex );
			void ActionCallback_Pinch( SAction *pAction, uint32_t unActionStateIndex );
			void ActionCallback_Grasp( SAction *pAction, uint32_t unActionStateIndex );

			void ActionHaptic( SAction *pAction, uint32_t unActionStateIndex ) const;

			struct SAssets
			{
				CRenderModel *pSky = nullptr;
				CRenderModel *pFloor = nullptr;
			}assets;

			struct SGameState
			{
				ERenderMode currentRenderMode = ERenderMode::Unlit;
				ETonemapOperator currentToneMapper = ETonemapOperator::None;

				uint32_t skyMateriaDataId = 0;
				uint32_t floorMateriaDataId = 0;
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

			// Plane facing user for passthrough mesh projection
			std::vector< XrVector3f > m_vecVertices;
			std::vector< uint32_t > m_vecIndices;

			std::unique_ptr < EXT::CHandJointsMotionRange > m_pHandsJointsMotionRange = nullptr;
			std::unique_ptr < META::CHandsAndControllers > m_pHandsAndControllers = nullptr;

	};

} // namespace xrapp