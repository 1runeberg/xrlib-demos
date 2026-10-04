// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include "aim_ray.hpp"
#include <cstring>
#include <xrlib.hpp>

namespace panel_input
{
	static void Check( XrResult result )
	{
		if ( XR_FAILED( result ) )
			throw result;
	}

	static XrPath Path( XrInstance instance, const char *name )
	{
		XrPath result {};
		Check( xrStringToPath( instance, name, &result ) );
		return result;
	}

	// Simple-controller selection rays for panel buttons and pinch scrolling
	struct SPanelInput
	{
		XrActionSet set {};
		XrAction select {}, aim {};
		XrPath hands[ 2 ] {};
		XrSpace spaces[ 2 ] {};
		SAimRay::SSample rays[ 2 ];
		XrPath profiles[ 2 ] {};
		bool controllers[ 2 ] {};
		struct SGesture
		{
			bool down = false, dragging = false, scrollable = false;
			bool pending = false, armed = false, recovering = false, cancelled = false;
			XrTime lastPoseTime = 0;
			int side = 0, target = SPanelLayout::none, scrollTarget = SPanelLayout::none;
			float startY = 0, lastY = 0;
		};
		SGesture gestures[ 2 ];

		~SPanelInput()
		{
			for ( auto space : spaces )
				if ( space )
					xrDestroySpace( space );

			if ( set )
				xrDestroyActionSet( set );
		}

		void Init( xrlib::CSession &session )
		{
			auto instance = session.GetAppInstance()->GetXrInstance();
			hands[ 0 ] = Path( instance, "/user/hand/left" );
			hands[ 1 ] = Path( instance, "/user/hand/right" );
			XrActionSetCreateInfo setInfo { XR_TYPE_ACTION_SET_CREATE_INFO };
			std::strcpy( setInfo.actionSetName, "scene" );
			std::strcpy( setInfo.localizedActionSetName, "Scene" );
			Check( xrCreateActionSet( instance, &setInfo, &set ) );
			XrActionCreateInfo action { XR_TYPE_ACTION_CREATE_INFO };
			action.countSubactionPaths = 2;
			action.subactionPaths = hands;
			action.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
			std::strcpy( action.actionName, "select" );
			std::strcpy( action.localizedActionName, "Select" );
			Check( xrCreateAction( set, &action, &select ) );
			action.actionType = XR_ACTION_TYPE_POSE_INPUT;
			std::strcpy( action.actionName, "aim_pose" );
			std::strcpy( action.localizedActionName, "Aim pose" );
			Check( xrCreateAction( set, &action, &aim ) );
			struct SProfile
			{
				const char *name;
				const char *select;
			};
			for ( const auto &profile :
				  { SProfile { "/interaction_profiles/khr/simple_controller", "/input/select/click" },
					SProfile { "/interaction_profiles/oculus/touch_controller", "/input/trigger/value" },
					SProfile { "/interaction_profiles/htc/vive_controller", "/input/trigger/click" },
					SProfile { "/interaction_profiles/valve/index_controller", "/input/trigger/value" },
					SProfile { "/interaction_profiles/microsoft/motion_controller", "/input/trigger/value" } } )
			{
				const std::string left = std::string( "/user/hand/left" ) + profile.select;
				const std::string right = std::string( "/user/hand/right" ) + profile.select;
				XrActionSuggestedBinding bindings[] = {
					{ select, Path( instance, left.c_str() ) }, { select, Path( instance, right.c_str() ) }, { aim, Path( instance, "/user/hand/left/input/aim/pose" ) }, { aim, Path( instance, "/user/hand/right/input/aim/pose" ) } };
				XrInteractionProfileSuggestedBinding suggested { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
				suggested.interactionProfile = Path( instance, profile.name );
				suggested.countSuggestedBindings = 4;
				suggested.suggestedBindings = bindings;
				const auto result = xrSuggestInteractionProfileBindings( instance, &suggested );
				if ( result != XR_ERROR_PATH_UNSUPPORTED )
					Check( result );
			}

			XrSessionActionSetsAttachInfo attach { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
			attach.countActionSets = 1;
			attach.actionSets = &set;
			Check( xrAttachSessionActionSets( session.GetXrSession(), &attach ) );
			for ( int i = 0; i < 2; ++i )
			{
				XrActionSpaceCreateInfo info { XR_TYPE_ACTION_SPACE_CREATE_INFO };
				info.action = aim;
				info.subactionPath = hands[ i ];
				info.poseInActionSpace.orientation.w = 1;
				Check( xrCreateActionSpace( session.GetXrSession(), &info, &spaces[ i ] ) );
			}
		}

		// Send completed clicks and scroll deltas to the demo on the render thread
		template < typename Click, typename Scroll, typename Hover > void Update( const SSceneState &scene, xrlib::CSession &session, XrTime time, const std::vector< SPanelRegion > &regions, Click click, Scroll scroll, Hover hover )
		{
			rays[ 0 ] = rays[ 1 ] = {};

			XrActiveActionSet active { set, XR_NULL_PATH };
			XrActionsSyncInfo sync { XR_TYPE_ACTIONS_SYNC_INFO };
			sync.countActiveActionSets = 1;
			sync.activeActionSets = &active;
			const auto result = xrSyncActions( session.GetXrSession(), &sync );
			if ( result == XR_SESSION_NOT_FOCUSED || !scene.placed )
			{
				for ( int hand = 0; hand < 2; ++hand )
				{
					gestures[ hand ] = {};
					hover( hand, 0, SPanelLayout::none );
				}
				return;
			}

			Check( result );
			for ( int hand = 0; hand < 2; ++hand )
			{
				auto &gesture = gestures[ hand ];
				XrActionStateGetInfo get { XR_TYPE_ACTION_STATE_GET_INFO };
				get.action = aim;
				get.subactionPath = hands[ hand ];
				XrActionStatePose pose { XR_TYPE_ACTION_STATE_POSE };
				Check( xrGetActionStatePose( session.GetXrSession(), &get, &pose ) );
				XrSpaceLocation location { XR_TYPE_SPACE_LOCATION };
				Check( xrLocateSpace( spaces[ hand ], session.GetAppSpace(), time, &location ) );

				XrInteractionProfileState profile { XR_TYPE_INTERACTION_PROFILE_STATE };
				Check( xrGetCurrentInteractionProfile( session.GetXrSession(), hands[ hand ], &profile ) );
				if ( profiles[ hand ] != profile.interactionProfile )
				{
					profiles[ hand ] = profile.interactionProfile;
					controllers[ hand ] = false;
					if ( profile.interactionProfile != XR_NULL_PATH )
					{
						char name[ XR_MAX_PATH_LENGTH ] {};
						uint32_t count = 0;
						Check( xrPathToString( session.GetAppInstance()->GetXrInstance(), profile.interactionProfile, sizeof( name ), &count, name ) );
						controllers[ hand ] = SAimRay::IsController( name );
					}
				}

				rays[ hand ] = { location.pose, location.locationFlags, pose.isActive == XR_TRUE, controllers[ hand ] };
				get.action = select;
				get.subactionPath = hands[ hand ];
				XrActionStateBoolean state { XR_TYPE_ACTION_STATE_BOOLEAN };
				Check( xrGetActionStateBoolean( session.GetXrSession(), &get, &state ) );
				if ( !state.isActive )
				{
					gesture = {};
					hover( hand, 0, SPanelLayout::none );
					continue;
				}

				const auto hit = [ & ]( int side, float x, float y ) { return SPanelLayout::Hit( regions, side, x, y ); };
				const auto exists = [ & ]( int side, int target ) { return target != SPanelLayout::none && std::any_of( regions.begin(), regions.end(), [ & ]( const auto &region ) { return region.side == side && region.target == target; } ); };
				const auto valid = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
				const bool validPose = pose.isActive && ( location.locationFlags & valid ) == valid;

				// Keep brief pose gaps recoverable without allowing a stale pinch to activate a control
				constexpr XrDuration recoveryWindow = 150'000'000;
				if ( gesture.down && ( time < gesture.lastPoseTime || time - gesture.lastPoseTime > recoveryWindow ) )
					gesture.cancelled = true;

				float x = 0, y = 0;
				int hoverSide = 0, hoverTarget = SPanelLayout::none;
				if ( validPose )
					for ( int side : { -1, 1 } )
						if ( SPanelLayout::Intersect( scene, static_cast< float >( side ), location.pose, x, y ) )
						{
							hoverSide = side;
							hoverTarget = hit( side, x, y );
							break;
						}

				if ( !state.currentState )
				{

					// Native pinch aim can become inactive on release, retain the last held hit in that case
					const bool releaseHit = validPose ? hoverSide == gesture.side && hoverTarget == gesture.target : gesture.armed;
					if ( gesture.down && !gesture.cancelled && !gesture.dragging && !gesture.recovering && releaseHit && state.changedSinceLastSync && exists( gesture.side, gesture.target ) )
						click( gesture.side, gesture.target );

					gesture = {};
					hover( hand, hoverSide, hoverTarget );
					continue;
				}

				if ( !gesture.down )
				{
					gesture.down = true;
					gesture.pending = true;
					gesture.lastPoseTime = time;
				}

				if ( !validPose || gesture.cancelled )
				{
					gesture.armed = false;
					gesture.recovering = true;
					hover( hand, 0, SPanelLayout::none );
					continue;
				}

				gesture.lastPoseTime = time;
				if ( gesture.pending )
				{
					gesture.pending = false;
					gesture.side = hoverSide;
					gesture.target = hoverTarget;
					gesture.armed = hoverTarget != SPanelLayout::none;
					gesture.startY = gesture.lastY = y;
					gesture.scrollTarget = gesture.armed ? SPanelLayout::ListAt( hoverSide, x, y ) : SPanelLayout::none;
					gesture.scrollable = exists( hoverSide, gesture.scrollTarget );
				}
				else if ( gesture.side && SPanelLayout::Project( scene, static_cast< float >( gesture.side ), location.pose, x, y ) )
				{

					// Rebase after a pose gap so recovery doesn't cause a scroll jump
					if ( gesture.recovering )
						gesture.startY = gesture.lastY = y;

					gesture.scrollable = gesture.scrollable && exists( gesture.side, gesture.scrollTarget );
					if ( gesture.scrollable && !gesture.dragging && std::abs( y - gesture.startY ) > .01f )
					{
						gesture.dragging = true;
						gesture.lastY = gesture.startY;
					}

					if ( gesture.dragging && gesture.scrollable )
						scroll( gesture.side, gesture.scrollTarget, y - gesture.lastY );

					gesture.armed = gesture.target != SPanelLayout::none && hit( gesture.side, x, y ) == gesture.target;
					gesture.lastY = y;
				}
				else
					gesture.armed = false;

				gesture.recovering = false;

				if ( gesture.dragging )
					hover( hand, gesture.side, gesture.scrollTarget );
				else
					hover( hand, hoverSide, hoverTarget );
			}
		}
	};
} // namespace panel_input
