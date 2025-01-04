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
#include <xrlib/ext/EXT/hand_tracking.hpp>

#define APP_NAME "inputxr"
using namespace app;

static const float k_bladeScaleHapticThreshold = 0.035f;

#ifdef XR_USE_PLATFORM_ANDROID
void android_main( struct android_app *pAndroidApp )
{
	// (1) Create App
	std::unique_ptr< App > pApp = std::make_unique< App >( 
		pAndroidApp, 
		APP_NAME,
		XR_MAKE_VERSION32( 1, 0, 0 ), 
		ELogLevel::LogVerbose );

    // (2) Initialize app
    pApp->Init();

#else
int main( int argc, char *argv[] )
{
	// (1) Create app
	std::unique_ptr< App > pApp = std::make_unique< App >( argc, argv, APP_NAME, XR_MAKE_VERSION32( 1, 0, 0 ), ELogLevel::LogVerbose );

    // (2) Initialize app
	int result = pApp->Init();
	if ( result == EXIT_FAILURE )
		return result;
#endif

	// (3) Setup scene
	pApp->SetupScene();

	// (4) Setup input

	// (4.1) Retrieve input object from provider - this is created during provider init()
	pApp->pInput = std::make_unique< CInput >( pApp->GetInstance() );
	pApp->pInput->Init( pApp->GetSession() );
	CInput *pInput = pApp->pInput.get();
	
	if ( !pInput )
	{
		LogError( APP_NAME, "Error creating input module." );
        #ifndef XR_USE_PLATFORM_ANDROID
                return XR_ERROR_INITIALIZATION_FAILED;
        #endif
	}

	// (4.2) Create actionset(s) - these are collections of actions that can be activated based on game state
	//								(e.g. one actionset for locomotion and another for ui handling)
	//								you can optionally provide a priority and other info (e.g. via extensions)
	SActionSet actionsetMain {};
	pInput->CreateActionSet( &actionsetMain, "main", "main actions" );

	// (4.3) Create action(s) - these represent actions that will be triggered based on hardware state from the openxr runtime

	SAction actionHiltPose( XR_ACTION_TYPE_POSE_INPUT, std::bind( &App::ActionCallback_SetControllerActive, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pInput->CreateAction( &actionHiltPose, &actionsetMain, "hilt_pose", "hilt pose", { "/user/hand/left", "/user/hand/right" } );
	
	SAction actionBladePose( XR_ACTION_TYPE_POSE_INPUT, std::bind( &App::ActionCallback_SetControllerActive, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pInput->CreateAction( &actionBladePose, &actionsetMain, "blade_pose", "blade pose", { "/user/hand/left", "/user/hand/right" } );

	SAction actionScaleBlade( XR_ACTION_TYPE_FLOAT_INPUT, std::bind( &App::ActionCallback_ScaleBlade, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pInput->CreateAction( &actionScaleBlade, &actionsetMain, "scale_blade", "scale blade", { "/user/hand/left", "/user/hand/right" } );

	SAction actionCycleRenderMode( XR_ACTION_TYPE_BOOLEAN_INPUT, std::bind( &App::ActionCallback_CycleRenderMode, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pInput->CreateAction( &actionCycleRenderMode, &actionsetMain, "cycle_render_mode", "Cycle render mode and tonemapping", { "/user/hand/left", "/user/hand/right" } );

	SAction actionTogglePassthrough( XR_ACTION_TYPE_BOOLEAN_INPUT, std::bind( &App::ActionCallback_TogglePassthrough, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pInput->CreateAction( &actionTogglePassthrough, &actionsetMain, "toggle_passthrough", "Toggle passthrough", { "/user/hand/left", "/user/hand/right" } );

	SAction actionHaptic( XR_ACTION_TYPE_VIBRATION_OUTPUT, std::bind( &App::ActionCallback_TogglePassthrough, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pApp->pHapticAction = &actionHaptic;
	pInput->CreateAction( pApp->pHapticAction, &actionsetMain, "haptic", "play haptics", { "/user/hand/left", "/user/hand/right" } );

	// (4.4) Create supported controllers
	ValveIndex controllerIndex {};
	OculusTouch controllerTouch {};
	HTCVive controllerVive {};
	MicrosoftMixedReality controllerMR {};

	// (4.5) Create action to controller bindings
	//		  The use of the "BaseController" here is optional. It's a convenience controller handle that will auto-map
	//		  basic controller components to every supported controller it knows of.
	//
	//		  Alternatively (or in addition), you can directly add bindings per controller
	//		  e.g. controllerVive.AddBinding(...)
	BaseController baseController {};
	baseController.vecSupportedControllers.push_back( &controllerIndex );
	baseController.vecSupportedControllers.push_back( &controllerTouch );
	baseController.vecSupportedControllers.push_back( &controllerVive );
	baseController.vecSupportedControllers.push_back( &controllerMR );

	// Poses
	pInput->AddBinding( &baseController, actionHiltPose.xrActionHandle, XR_HAND_LEFT_EXT, InputComponent::GripPose, InputQualifier::None );
	pInput->AddBinding( &baseController, actionHiltPose.xrActionHandle, XR_HAND_RIGHT_EXT, InputComponent::GripPose, InputQualifier::None );

	pInput->AddBinding( &baseController, actionBladePose.xrActionHandle, XR_HAND_LEFT_EXT, InputComponent::GripPose, InputQualifier::None );
	pInput->AddBinding( &baseController, actionBladePose.xrActionHandle, XR_HAND_RIGHT_EXT, InputComponent::GripPose, InputQualifier::None );

	// Blade scaling
	pInput->AddBinding( &baseController, actionScaleBlade.xrActionHandle, XR_HAND_LEFT_EXT, InputComponent::Trigger, InputQualifier::Click );
	pInput->AddBinding( &baseController, actionScaleBlade.xrActionHandle, XR_HAND_RIGHT_EXT, InputComponent::Trigger, InputQualifier::Click );

	// Button actions
	pInput->AddBinding( &baseController, actionCycleRenderMode.xrActionHandle, XR_HAND_LEFT_EXT, InputComponent::PrimaryButton, InputQualifier::Click );
	pInput->AddBinding( &baseController, actionCycleRenderMode.xrActionHandle, XR_HAND_RIGHT_EXT, InputComponent::PrimaryButton, InputQualifier::Click );

	pInput->AddBinding( &baseController, actionTogglePassthrough.xrActionHandle, XR_HAND_LEFT_EXT, InputComponent::SecondaryButton, InputQualifier::Click );
	pInput->AddBinding( &baseController, actionTogglePassthrough.xrActionHandle, XR_HAND_RIGHT_EXT, InputComponent::SecondaryButton, InputQualifier::Click );

	// Haptics
	pInput->AddBinding( &baseController, actionHaptic.xrActionHandle, XR_HAND_LEFT_EXT, InputComponent::Haptic, InputQualifier::None );
	pInput->AddBinding( &baseController, actionHaptic.xrActionHandle, XR_HAND_RIGHT_EXT, InputComponent::Haptic, InputQualifier::None );

	// (4.6) Suggest bindings to the active openxr runtime
	//        As with adding bindings, you can also suggest bindings manually per controller
	//        e.g. controllerIndex.SuggestBindings(...)
	pInput->SuggestBindings( &baseController, nullptr );

	std::vector< XrActionSet > vecActionSets = { actionsetMain.xrActionSetHandle };
	pInput->AttachActionSetsToSession( vecActionSets.data(), static_cast< uint32_t >( vecActionSets.size() ) );

	// (4.7) Add actionset(s) that will be active during input sync with the openxr runtime
	//		  these are the action sets (and their actions) whose state will be checked in the successive frames.
	//		  you can change this anytime (e.g. changing game mode to locomotion vs ui)
	pInput->AddActionsetForSync( &actionsetMain ); //  optional sub path is a filter if made available with the action - e.g /user/hand/left

	// (4.8) Create action spaces for pose actions - this is the opportunity to tweak the base pose of the mesh for intended use
	XrPosef poseSpace;
	XrPosef_CreateIdentity( &poseSpace );

	pInput->CreateActionSpaces( &actionHiltPose, &poseSpace );
	pInput->CreateActionSpaces( &actionBladePose, &poseSpace );

	// assign the action space to models
	pApp->assets.pHiltLeft->instances[ 0 ].space = actionHiltPose.vecActionSpaces[ 0 ];
	pApp->assets.pButtonBottomLeft->instances[ 0 ].space = actionHiltPose.vecActionSpaces[ 0 ];
	pApp->assets.pButtonTopLeft->instances[ 0 ].space = actionHiltPose.vecActionSpaces[ 0 ];
	
	pApp->assets.pHiltRight->instances[ 0 ].space = actionHiltPose.vecActionSpaces[ 1 ];
	pApp->assets.pButtonBottomRight->instances[ 0 ].space = actionHiltPose.vecActionSpaces[ 1 ];
	pApp->assets.pButtonTopRight->instances[ 0 ].space = actionHiltPose.vecActionSpaces[ 1 ];
	
	pApp->assets.pBladeLeft->instances[ 0 ].space = actionBladePose.vecActionSpaces[ 0 ];
	pApp->assets.pBladeRight->instances[ 0 ].space = actionBladePose.vecActionSpaces[ 1 ];


	// (5) Render loop
	while ( pApp->GetSession()->GetState() != XR_SESSION_STATE_EXITING )
	{
		// (5.1) Poll for xr events
		XrEventDataBaseHeader xrEventDataBaseheader { XR_TYPE_EVENT_DATA_EVENTS_LOST };
		if ( !XR_SUCCEEDED( pApp->GetSession()->Poll( &xrEventDataBaseheader ) ) )
			continue;

		// (5.2) Process xr events
		pApp->ProcessXrEvents( xrEventDataBaseheader );

		// (5.3) Get thread pool manager
		CThreadPool *pThreadPool = pApp->pThreadPool.get();

		// (5.4) Input Frame
		if ( pInput )
		{
			pThreadPool->SubmitInputTask( [ pInput = pInput ]() { pInput->ProcessInput(); } ).get();

			if ( pApp->assets.pBladeLeft->instances[ 0 ].scale.z > k_bladeScaleHapticThreshold )
				pApp->ActionHaptic( &actionHaptic, 0 );

			if ( pApp->assets.pBladeRight->instances[ 0 ].scale.z > k_bladeScaleHapticThreshold )
				pApp->ActionHaptic( &actionHaptic, 1 );
		}

		// (5.5) Render frame
		bool bFrameStarted = pThreadPool->SubmitRenderTask( [ pApp = pApp.get() ]() { return pApp->StartRenderFrame(); } ).get();

		if ( bFrameStarted && pApp->pRenderInfo->state.frameState.shouldRender )
		{
			// Update sky animation
			float displayRate = pApp->GetDisplayRate() ? 
				pApp->GetDisplayRate()->GetCurrentRefreshRate( pApp->GetSession()->GetXrSession() ) : 72.0f;

			pApp->skyanim.UpdateAnimation(
				pApp->assets, 
				pApp->pRenderInfo->state.frameState.predictedDisplayPeriod,
				displayRate,
				pApp->gamestate.skyMateriaDataId,
				pApp->gamestate.vecMaterialData	);

			auto now = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration< float >( now.time_since_epoch() );
			float timeInSeconds = duration.count();
			pApp->gamestate.vecMaterialData[ pApp->gamestate.skyMateriaDataId ]->emissiveFactor[ 0 ] = timeInSeconds;

			// Update player position for floor marker
			XrVector3f hmdLocation = pApp->pRenderInfo->state.hmdPose.position;
			pApp->gamestate.vecMaterialData[ pApp->gamestate.floorMateriaDataId ]->emissiveFactor[ 0 ] = hmdLocation.x;
			pApp->gamestate.vecMaterialData[ pApp->gamestate.floorMateriaDataId ]->emissiveFactor[ 1 ] = hmdLocation.z;

			// Update plasma blade effect
			pApp->plasma.UpdateEffect( 
				pApp->gamestate.vecMaterialData[ pApp->gamestate.leftBladeMateriaDataId ],
				pApp->gamestate.vecMaterialData[ pApp->gamestate.rightBladeMateriaDataId ] );

		}

		// Submit EndRenderFrame to render thread and wait for it
		pThreadPool->SubmitRenderTask( [ pApp = pApp.get() ]() { pApp->EndRenderFrame(); } ).get();
	}

	// (6) Exit app - xrlib objects (instance, session, renderer, etc) handles proper cleanup once unique pointers goes out of scope.
	//				  Note that as they are in the same scope in this demo, order of destruction here is automatically enforced only when using C++20 and above
	std::cout << "\n\nPress enter to end.";
	std::cin.get();

	return EXIT_SUCCESS;
}
