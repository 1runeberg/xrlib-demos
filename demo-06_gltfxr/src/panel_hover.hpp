// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include "panel_state.hpp"
#include <vector>
#include <xrlib.hpp>

#ifndef XR_USE_PLATFORM_ANDROID
	#include <VisionOpenXR/vision_openxr.h>
#endif

// Optional system hover for the same regions used by OpenXR panel input
struct SPanelSystemHover
{
#ifndef XR_USE_PLATFORM_ANDROID
	XrSpace local = XR_NULL_HANDLE;

	~SPanelSystemHover()
	{
		if ( local )
			xrDestroySpace( local );
	}
#endif

	XrResult Submit( const SSceneState &scene, xrlib::CSession &session, XrTime time, const std::vector< SPanelRegion > &regions )
	{
#ifndef XR_USE_PLATFORM_ANDROID
		if ( !vxrCompositorSupportsHover() )
			return XR_SUCCESS;

		if ( !local )
		{
			XrReferenceSpaceCreateInfo info { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
			info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
			info.poseInReferenceSpace.orientation.w = 1;
			const auto result = xrCreateReferenceSpace( session.GetXrSession(), &info, &local );
			if ( XR_FAILED( result ) )
				return result;
		}

		XrSpaceLocation location { XR_TYPE_SPACE_LOCATION };
		const auto result = xrLocateSpace( session.GetAppSpace(), local, time, &location );
		if ( XR_FAILED( result ) )
			return result;

		std::vector< VxrHoverRegion > nativeRegions;
		const auto valid = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
		if ( scene.placed && ( location.locationFlags & valid ) == valid )
			for ( const auto &region : SPanelLayout::HoverRegions( regions ) )
			{
				const auto panel = SPanelLayout::Pose( scene, static_cast< float >( region.side ) );
				const auto centre = SSceneState::Rotate( panel.orientation, { ( region.left + region.right ) * .5f, ( region.bottom + region.top ) * .5f, .001f } );
				const auto position = SSceneState::Rotate( location.pose.orientation, { panel.position.x + centre.x, panel.position.y + centre.y, panel.position.z + centre.z } );
				const auto orientation = SSceneState::Multiply( location.pose.orientation, panel.orientation );

				// Keep identifiers stable when scrolling changes a list item's visible row
				const uint64_t target = region.target >= 0 ? static_cast< uint64_t >( region.target ) + 16 : static_cast< uint64_t >( -region.target );
				nativeRegions.push_back(
					{ target * 2 + ( region.side > 0 ? 1u : 0u ),
					  { orientation.x, orientation.y, orientation.z, orientation.w },
					  { position.x + location.pose.position.x, position.y + location.pose.position.y, position.z + location.pose.position.z },
					  { region.right - region.left, region.top - region.bottom } } );
			}

		return static_cast< XrResult >( vxrCompositorSetHoverRegions( static_cast< uint32_t >( nativeRegions.size() ), nativeRegions.data() ) );
#else
		return XR_SUCCESS;
#endif
	}
};
