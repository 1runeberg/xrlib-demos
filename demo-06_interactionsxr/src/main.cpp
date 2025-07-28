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

using namespace app;

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
	if( !pApp->GetInstance() ||
		!pApp->GetSession() ||
		pApp->GetInstance()->GetXrInstance() == XR_NULL_HANDLE ||
		pApp->GetSession()->GetXrSession() == XR_NULL_HANDLE )
			return xrlib::ExitApp( pAndroidApp );

#else
int main( int argc, char *argv[] )
{
	// (1) Create app
	std::unique_ptr< App > pApp = std::make_unique< App >( argc, argv, APP_NAME, XR_MAKE_VERSION32( 1, 0, 0 ), ELogLevel::LogVerbose );

    // (2) Initialize app
	if ( pApp->Init() == EXIT_FAILURE )
		return xrlib::ExitApp( EXIT_FAILURE );
#endif

	// This demo requires the EXT hand tracking extension to be enabled
	if( !pApp->GetInstance()->IsExtensionEnabled( XR_EXT_HAND_INTERACTION_EXTENSION_NAME ) )
	{
		LogError( APP_NAME, "EXT hand interaction extension is not enabled. This demo requires it." );
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif
	}

	// (3) Setup scene
	pApp->SetupScene();

	// (3.1) Set standard scales
	XrVector3f zeroScale { 0.f, 0.f, 0.f };
	XrVector3f idScale { 1.f, 1.f, 1.f };
	XrVector3f controllerScale { 0.035f, 0.035f, 0.075f };
	XrVector3f pinchScale { 0.025f, 0.025f, 0.025f };

	// (3.2) Define hand joint locations and debug indicators
	EXT::CHandTracking::SJointLocations jointLocations;
	CColoredPyramid *debugIndicator = nullptr;
	{
		debugIndicator = new CColoredPyramid( pApp->GetSession(), pApp->pRenderInfo.get(), pApp->pipelines.primitiveLayout, pApp->pipelines.primitives );
		debugIndicator->AddInstance( ( XR_HAND_JOINT_COUNT_EXT * 2 ) - 1 );
		debugIndicator->InitBuffers();

		pApp->pRenderInfo->AddNewRenderable( dynamic_cast< CRenderable * >( debugIndicator ) );
	}

	// (3.3) Add controller debug indicators
	CColoredCube *debugControllerIndicator = nullptr;
	{
		debugControllerIndicator = new CColoredCube(
			pApp->GetSession(),
			pApp->pRenderInfo.get(),
			pApp->pipelines.primitiveLayout,
			pApp->pipelines.primitives,
			std::numeric_limits< uint32_t >::max(), true, 0.5f, zeroScale );
		debugControllerIndicator->AddInstance( 1, zeroScale );
		debugControllerIndicator->InitBuffers();

		pApp->pRenderInfo->AddNewRenderable( dynamic_cast< CRenderable * >( debugControllerIndicator ) );
	}

	// (3.4) Add pinch debug indicator
	CColoredCube *debugPinchIndicator = nullptr;
	{
		debugPinchIndicator = new CColoredCube(
			pApp->GetSession(),
			pApp->pRenderInfo.get(),
			pApp->pipelines.primitiveLayout,
			pApp->pipelines.primitives,
			std::numeric_limits< uint32_t >::max(), true, 0.5f, pinchScale );
		debugPinchIndicator->AddInstance( 1, pinchScale );
		debugPinchIndicator->InitBuffers();

		pApp->pRenderInfo->AddNewRenderable( dynamic_cast< CRenderable * >( debugPinchIndicator ) );
	}

	// (3.5) Add window
	CColoredPlane *debugWindow = nullptr;
	{
		debugWindow = new CColoredPlane( pApp->GetSession(), pApp->pRenderInfo.get(), pApp->pipelines.primitiveLayout, pApp->pipelines.primitives, true );
		debugWindow->InitBuffers();

		debugWindow->instances[ 0 ].scale = zeroScale;
		debugWindow->instances[ 0 ].pose = k_windowPose;
		pApp->pRenderInfo->AddNewRenderable( dynamic_cast< CRenderable * >( debugWindow ) );
	}

	// (4) Setup input

	// (4.1) Retrieve input object from provider - this is created during provider init()
	pApp->pInput = std::make_unique< CInput >( pApp->GetInstance() );
	pApp->pInput->Init( pApp->GetSession() );
	
	if ( !pApp->pInput )
	{
		LogError( APP_NAME, "Error creating input module." );
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp( EXIT_FAILURE );
        #endif
	}

	// (4.2) Create actionset(s) - these are collections of actions that can be activated based on game state
	//								(e.g. one actionset for locomotion and another for ui handling)
	//								you can optionally provide a priority and other info (e.g. via extensions)
	SActionSet actionsetMain {};
	pApp->pInput->CreateActionSet( &actionsetMain, "main", "main actions" );

	// (4.3) Create action(s) - these represent actions that will be triggered based on hardware state from the openxr runtime

	SAction actionControllerPose( XR_ACTION_TYPE_POSE_INPUT, std::bind( &App::ActionCallback_SetControllerActive, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pApp->pInput->CreateAction( &actionControllerPose, &actionsetMain, "controller_pose", "controller pose", { "/user/hand/left", "/user/hand/right" } );

	SAction actionPinchPose( XR_ACTION_TYPE_POSE_INPUT, std::bind( &App::ActionCallback_NoAction, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pApp->pInput->CreateAction( &actionPinchPose, &actionsetMain, "pinch_pose", "pinch pose", { "/user/hand/left", "/user/hand/right" } );

	SAction actionWindowPose( XR_ACTION_TYPE_POSE_INPUT, std::bind( &App::ActionCallback_NoAction, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pApp->pInput->CreateAction( &actionWindowPose, &actionsetMain, "window_pose", "window pose" );

	SAction actionPinch( XR_ACTION_TYPE_FLOAT_INPUT, std::bind( &App::ActionCallback_Pinch, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pApp->pInput->CreateAction( &actionPinch, &actionsetMain, "pinch", "pinch strength", { "/user/hand/left", "/user/hand/right" } );

	SAction actionGrasp( XR_ACTION_TYPE_FLOAT_INPUT, std::bind( &App::ActionCallback_Grasp, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pApp->pInput->CreateAction( &actionGrasp, &actionsetMain, "grasp", "grasp strength", { "/user/hand/left", "/user/hand/right" } );

	SAction actionHaptic( XR_ACTION_TYPE_VIBRATION_OUTPUT, std::bind( &App::ActionCallback_NoAction, pApp.get(), fnArg::_1, fnArg::_2 ) );
	pApp->pHapticAction = &actionHaptic;
	pApp->pInput->CreateAction( pApp->pHapticAction, &actionsetMain, "haptic", "play haptics", { "/user/hand/left", "/user/hand/right" } );

	// (4.4) Create supported controllers - most new controllers typically support input paths from this mix of base controllers
	ValveIndex controllerIndex {};
	OculusTouch controllerTouch {};
	HTCVive controllerVive {};
	MicrosoftMixedReality controllerMR {};

	SHandInteraction handInteraction {};	// Custom controller type provided by the hand interaction extension

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
	pApp->pInput->AddBinding( &baseController, actionControllerPose.xrActionHandle, XR_HAND_LEFT_EXT, InputComponent::GripPose, InputQualifier::None );
	pApp->pInput->AddBinding( &baseController, actionControllerPose.xrActionHandle, XR_HAND_RIGHT_EXT, InputComponent::GripPose, InputQualifier::None );

	// Haptics
	pApp->pInput->AddBinding( &baseController, actionHaptic.xrActionHandle, XR_HAND_LEFT_EXT, InputComponent::Haptic, InputQualifier::None );
	pApp->pInput->AddBinding( &baseController, actionHaptic.xrActionHandle, XR_HAND_RIGHT_EXT, InputComponent::Haptic, InputQualifier::None );

	// Add bindings for hand interaction extension
	handInteraction.AddBinding( pApp->GetInstance()->GetXrInstance(), actionPinchPose.xrActionHandle, XR_HAND_LEFT_EXT, EXT::CHandInteraction::EHandInteractionComponent::PinchPose );
	handInteraction.AddBinding( pApp->GetInstance()->GetXrInstance(), actionPinchPose.xrActionHandle, XR_HAND_RIGHT_EXT, EXT::CHandInteraction::EHandInteractionComponent::PinchPose );

	handInteraction.AddBinding( pApp->GetInstance()->GetXrInstance(), actionPinch.xrActionHandle, XR_HAND_LEFT_EXT, EXT::CHandInteraction::EHandInteractionComponent::PinchValue );
	handInteraction.AddBinding( pApp->GetInstance()->GetXrInstance(), actionPinch.xrActionHandle, XR_HAND_RIGHT_EXT, EXT::CHandInteraction::EHandInteractionComponent::PinchValue );

	handInteraction.AddBinding( pApp->GetInstance()->GetXrInstance(), actionGrasp.xrActionHandle, XR_HAND_LEFT_EXT, EXT::CHandInteraction::EHandInteractionComponent::GraspValue );
	handInteraction.AddBinding( pApp->GetInstance()->GetXrInstance(), actionGrasp.xrActionHandle, XR_HAND_RIGHT_EXT, EXT::CHandInteraction::EHandInteractionComponent::GraspValue );

	// (4.6) Suggest bindings to the active openxr runtime
	//        As with adding bindings, you can also suggest bindings manually per controller
	//        e.g. controllerIndex.SuggestBindings(...)
	if ( !XR_SUCCEEDED( pApp->pInput->SuggestBindings( &baseController, nullptr ) ) ||
		 !XR_SUCCEEDED( pApp->pInput->SuggestBindings( &handInteraction, nullptr ) ) )
	{
        #ifdef XR_USE_PLATFORM_ANDROID
            return xrlib::ExitApp( pAndroidApp );
        #else
            return xrlib::ExitApp ( EXIT_FAILURE );
        #endif
	}

	std::vector< XrActionSet > vecActionSets = { actionsetMain.xrActionSetHandle };
	pApp->pInput->AttachActionSetsToSession( vecActionSets.data(), static_cast< uint32_t >( vecActionSets.size() ) );

	// (4.7) Add actionset(s) that will be active during input sync with the openxr runtime
	//		  these are the action sets (and their actions) whose state will be checked in the successive frames.
	//		  you can change this anytime (e.g. changing game mode to locomotion vs ui)
	pApp->pInput->AddActionsetForSync( &actionsetMain ); //  optional sub path is a filter if made available with the action - e.g /user/hand/left

	// (4.8) Create action spaces for pose actions - this is the opportunity to tweak the base pose of the mesh for intended use
	XrPosef poseSpace;
	XrPosef_CreateIdentity( &poseSpace );

	pApp->pInput->CreateActionSpaces( &actionControllerPose, &poseSpace );
	pApp->pInput->CreateActionSpaces( &actionPinchPose, &poseSpace );

	// assign the action space to models
	debugControllerIndicator->instances[ 0 ].space = actionControllerPose.vecActionSpaces[ 0 ];
	debugPinchIndicator->instances[ 0 ].space = actionPinchPose.vecActionSpaces[ 0 ];

	debugControllerIndicator->instances[ 1 ].space = actionControllerPose.vecActionSpaces[ 1 ];
	debugPinchIndicator->instances[ 1 ].space = actionPinchPose.vecActionSpaces[ 1 ];

	// (5) Game loop
	while ( pApp->GetSession()->GetState() != XR_SESSION_STATE_EXITING )
	{
		// (5.1) Poll for xr events
		XrEventDataBaseHeader xrEventDataBaseheader { XR_TYPE_EVENT_DATA_EVENTS_LOST };
		if ( !XR_SUCCEEDED( pApp->GetSession()->Poll( &xrEventDataBaseheader ) ) )
			continue;

		// (5.2) Process xr events
		pApp->ProcessXrEvents( xrEventDataBaseheader );

		// (5.4) Input Frame
		pApp->pThreadPool->SubmitInputTask( [ pInput = pApp->pInput.get() ]() { pInput->ProcessInput(); } ).get();

		// Hide/Show controller indicators
		debugControllerIndicator->instances[ 0 ].scale = pApp->gamestate.bLeftControllerActive ? controllerScale : zeroScale;
		debugControllerIndicator->instances[ 1 ].scale = pApp->gamestate.bRightControllerActive ? controllerScale : zeroScale;

		// Hide/Show pinch indicators
		XrVector3f_Scale( &debugPinchIndicator->instances[ 0 ].scale, &pinchScale, pApp->gamestate.leftPinchStrength );
		XrVector3f_Scale( &debugPinchIndicator->instances[ 1 ].scale, &pinchScale, pApp->gamestate.rightPinchStrength );

		// Hide/Show window
		float graspStrength = pApp->gamestate.leftGraspStrength + pApp->gamestate.rightGraspStrength;
		if( pApp->GetPassthrough() && !pApp->GetPassthrough()->GetGeometryInstances()->empty() )
		{
			XrVector3f newScale = zeroScale;
			XrVector3f_Scale( &newScale, &idScale, graspStrength );

			pApp->GetPassthrough()->UpdateGeometry(
				pApp->GetPassthrough()->GetGeometryInstances()->at(0),
				pApp->GetSession()->GetAppSpace(),
				pApp->pRenderInfo->state.frameState.predictedDisplayTime,
				k_windowPose,
				newScale
				);
		}
		else
		{
			XrVector3f_Scale( &debugWindow->instances[ 0 ].scale, &idScale, graspStrength );
		}


		// Apply haptics
		if ( graspStrength > 1.f )
		{
			pApp->ActionHaptic( &actionHaptic, 0 );
			pApp->ActionHaptic( &actionHaptic, 1 );
		}


		// (5.5) Render frame
		bool bFrameStarted = pApp->pThreadPool->SubmitRenderTask( [ pApp = pApp.get() ]() { return pApp->StartRenderFrame(); } ).get();

		if ( bFrameStarted )
		{
			// Check conditions before submitting to thread pool
			if ( pApp->GetHandTracking() &&
				 pApp->pRenderInfo->state.frameState.shouldRender &&
				 ( pApp->pRenderInfo->state.sharedEyeState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT )
			)
			{
				// Check for joints motion range support
				if ( pApp->GetHandsJointsMotionRange() )
				{
					XrHandJointsMotionRangeInfoEXT motionRangeInfoLeft = pApp->gamestate.bLeftControllerActive ? EXT::CHandJointsMotionRange::GetHandJointsMotionRangeInfo( XR_HAND_JOINTS_MOTION_RANGE_CONFORMING_TO_CONTROLLER_EXT ) :
																									   EXT::CHandJointsMotionRange::GetHandJointsMotionRangeInfo( XR_HAND_JOINTS_MOTION_RANGE_UNOBSTRUCTED_EXT );

					XrHandJointsMotionRangeInfoEXT motionRangeInfoRight = pApp->gamestate.bRightControllerActive ? EXT::CHandJointsMotionRange::GetHandJointsMotionRangeInfo( XR_HAND_JOINTS_MOTION_RANGE_CONFORMING_TO_CONTROLLER_EXT ) :
																									     EXT::CHandJointsMotionRange::GetHandJointsMotionRangeInfo( XR_HAND_JOINTS_MOTION_RANGE_UNOBSTRUCTED_EXT );
					// Locate hand joints with motion range
					pApp->GetHandTracking()->LocateHandJoints( &jointLocations, pApp->GetSession()->GetAppSpace(), pApp->pRenderInfo->state.frameState.predictedDisplayTime, &motionRangeInfoLeft, &motionRangeInfoRight );
				}
				else
				{
					// Locate hand joints (can be done before waiting for the updates)
					pApp->GetHandTracking()->LocateHandJoints( &jointLocations, pApp->GetSession()->GetAppSpace(), pApp->pRenderInfo->state.frameState.predictedDisplayTime );
				}

				// Submit task for left hand joints
				auto leftHandUpdateFuture = pApp->pThreadPool->SubmitTask(
					[ pApp = pApp.get(), pHandtracking = pApp->GetHandTracking(), &debugIndicator, &jointLocations ]()
					{
						// Process left hand joints
						for ( size_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++ )
						{
							if ( jointLocations.leftJointLocations[ i ].locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT )
							{
								debugIndicator->instances[ i ].pose = jointLocations.leftJointLocations[ i ].pose;
								debugIndicator->ResetScale( jointLocations.leftJointLocations[ i ].radius, i );
							}
						}
					} );

				// Submit task for right hand joints
				auto rightHandUpdateFuture = pApp->pThreadPool->SubmitTask(
					[ pApp = pApp.get(), pHandtracking = pApp->GetHandTracking(), &debugIndicator, &jointLocations ]()
					{
						// Process right hand joints
						for ( size_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++ )
						{
							if ( jointLocations.rightJointLocations[ i ].locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT )
							{
								// Offset by XR_HAND_JOINT_COUNT_EXT to store right hand instances after left hand
								size_t rightHandIndex = i + XR_HAND_JOINT_COUNT_EXT;
								debugIndicator->instances[ rightHandIndex ].pose = jointLocations.rightJointLocations[ i ].pose;
								debugIndicator->ResetScale( jointLocations.rightJointLocations[ i ].radius, rightHandIndex );
							}
						}
					} );

				// Wait for both hand updates to complete
				leftHandUpdateFuture.wait();
				rightHandUpdateFuture.wait();
			}

			// Submit EndRenderFrame to render thread and wait for it
			pApp->pThreadPool->SubmitRenderTask( [ pApp = pApp.get() ]() { pApp->EndRenderFrame(); } ).get();
		}
	}

	// (6) Exit app - xrlib objects (instance, session, renderer, etc) handles proper cleanup once unique pointers goes out of scope.
	//				  Note that as they are in the same scope in this demo, order of destruction here is automatically enforced only when using C++20 and above
    #ifdef XR_USE_PLATFORM_ANDROID
        return xrlib::ExitApp( pAndroidApp );
    #else
        return xrlib::ExitApp( EXIT_FAILURE );
    #endif
}
