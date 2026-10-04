// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0

// Static model placement in the initial horizontal head direction

#pragma once

#include <algorithm>
#include <cmath>
#include <openxr/openxr.h>

// Model placement in the app's LOCAL space, in metres
struct SSceneState
{
	XrQuaternionf orientation { 0, 0, 0, 1 }; // Model orientation in the LOCAL space
	XrVector3f position { 0, 0, -1 };		  // Centre in the LOCAL space, set by the initial placement
	float floorY = -fallbackFloorDrop;		  // Grid floor height, the fallback until the runtime reports a floor
	bool placed = false;					  // Placement is retained across head movement, pauses and blend transitions
	static constexpr float distance = 1.5f;
	static constexpr float fallbackFloorDrop = 1.4f; // Between typical seated and standing eye heights
	static constexpr float size = .44f;				 // Model height in metres

	// Compose unit quaternions, applying right before left
	static XrQuaternionf Multiply( XrQuaternionf left, XrQuaternionf right )
	{
		return {
			left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
			left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
			left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
			left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z };
	}

	// Rotate using a unit quaternion, without scaling the vector
	static XrVector3f Rotate( XrQuaternionf rotation, XrVector3f vector )
	{
		auto result = Multiply( Multiply( rotation, { vector.x, vector.y, vector.z, 0 } ), { -rotation.x, -rotation.y, -rotation.z, rotation.w } );
		return { result.x, result.y, result.z };
	}

	// Anchor ahead of the initial head position at head height, so seated and standing views match
	// The caller checks tracking validity. Later calls preserve the anchor
	// Ignore pitch and return false if no horizontal heading is available
	bool Place( XrPosef head )
	{
		if ( placed )
			return true;

		if ( !std::isfinite( head.position.x ) || !std::isfinite( head.position.y ) || !std::isfinite( head.position.z ) )
			return false;

		auto forward = Rotate( head.orientation, { 0, 0, -1 } );
		const float length = std::hypot( forward.x, forward.z );

		if ( !std::isfinite( length ) || length < .001f )
			return false;

		forward = { forward.x / length, 0, forward.z / length };
		position = { head.position.x + forward.x * distance, head.position.y, head.position.z + forward.z * distance };
		floorY = head.position.y - fallbackFloorDrop;

		const float yaw = std::atan2( -forward.x, -forward.z );
		orientation = { 0, std::sin( yaw * .5f ), 0, std::cos( yaw * .5f ) };
		placed = true;

		return true;
	}

	// Move the grid floor to a reported floor below the model, keeping the last good height otherwise
	// The model and panels don't move, so floor corrections can't shift the scene
	// Returns true when the floor moved by more than a centimetre
	bool UpdateFloor( const XrSpaceLocation &floor )
	{
		const float y = floor.pose.position.y;
		if ( !placed || !( floor.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) || !std::isfinite( y ) || y >= position.y )
			return false;

		const bool moved = std::abs( y - floorY ) > .01f;
		floorY = y;

		return moved;
	}

	// Keep the info panel beside the model in the anchored scene orientation
	XrVector3f PanelPosition( float offsetX ) const
	{
		const auto offset = Rotate( orientation, { offsetX, 0, 0 } );
		return { position.x + offset.x, position.y + offset.y, position.z + offset.z };
	}
};
