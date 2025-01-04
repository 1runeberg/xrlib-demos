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

#define APP_NAME "handtrackingxr"
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

	// (3) Initialize hand tracking
	std::unique_ptr< EXT::CHandTracking > pHandtracking = nullptr;
	EXT::CHandTracking::SJointLocations jointLocations;
	if ( pApp->GetInstance()->IsExtensionEnabled( XR_EXT_HAND_TRACKING_EXTENSION_NAME ) )
	{
		pHandtracking = std::make_unique< EXT::CHandTracking >( pApp->GetInstance()->GetXrInstance() );
		
		if ( !XR_UNQUALIFIED_SUCCESS( pHandtracking->Init( pApp->GetSession()->GetXrSession() ) ) )
			pHandtracking.reset();
	}

	// (4) Setup scene
	pApp->SetupScene();

	// Add joint debug indicators
	CColoredPyramid *debugIndicator = nullptr;
	{
		debugIndicator = new CColoredPyramid( pApp->GetSession(), pApp->pRenderInfo.get(), pApp->pipelines.primitiveLayout, pApp->pipelines.primitives );
		debugIndicator->AddInstance( ( XR_HAND_JOINT_COUNT_EXT * 2 ) - 1 );
		debugIndicator->InitBuffers();
		
		pApp->pRenderInfo->AddNewRenderable( dynamic_cast< CRenderable * >( debugIndicator ) );
	}

	// (5) Render loop
	while ( pApp->GetSession()->GetState() != XR_SESSION_STATE_EXITING )
	{
		// (5.1) Poll for xr events
		XrEventDataBaseHeader xrEventDataBaseheader { XR_TYPE_EVENT_DATA_EVENTS_LOST };
		if ( !XR_SUCCEEDED( pApp->GetSession()->Poll( &xrEventDataBaseheader ) ) )
			continue;

		// (5.2) Process xr events
		pApp->ProcessXrEvents( xrEventDataBaseheader );

		// (5.3) Render frame
		CThreadPool *pThreadPool = pApp->pThreadPool.get();
		bool bFrameStarted = pThreadPool->SubmitRenderTask( [ pApp = pApp.get() ]() { return pApp->StartRenderFrame(); } ).get();

		if ( bFrameStarted )
		{
			// Check conditions before submitting to thread pool
			if ( pHandtracking && 
					pApp->pRenderInfo->state.frameState.shouldRender && 
					( pApp->pRenderInfo->state.sharedEyeState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT ) 
			   )
			{
				// Locate hand joints (can be done before waiting for the updates)
				pHandtracking->LocateHandJoints( &jointLocations, pApp->GetSession()->GetAppSpace(), pApp->pRenderInfo->state.frameState.predictedDisplayTime );

				// Submit task for left hand joints
				auto leftHandUpdateFuture = pThreadPool->SubmitTask(
					[ pApp = pApp.get(), pHandtracking = pHandtracking.get(), &debugIndicator, &jointLocations ]()
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
				auto rightHandUpdateFuture = pThreadPool->SubmitTask(
					[ pApp = pApp.get(), pHandtracking = pHandtracking.get(), &debugIndicator, &jointLocations ]()
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
			pThreadPool->SubmitRenderTask( [ pApp = pApp.get() ]() { pApp->EndRenderFrame(); } ).get();
		}

	}

	// (14) Exit app - xrlib objects (instance, session, renderer, etc) handles proper cleanup once unique pointers goes out of scope.
	//				  Note that as they are in the same scope in this demo, order of destruction here is automatically enforced only when using C++20 and above
	std::cout << "\n\nPress enter to end.";
	std::cin.get();

	return EXIT_SUCCESS;
}
