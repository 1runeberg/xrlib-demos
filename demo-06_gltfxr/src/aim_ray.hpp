// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include "panel_palette.hpp"
#include "panel_state.hpp"
#include <string_view>
#include <xrvk/mesh.hpp>

// App policy and geometry for an OpenXR aim ray
struct SAimRay
{
	struct SSample
	{
		XrPosef pose { { 0, 0, 0, 1 }, {} };
		XrSpaceLocationFlags flags = 0;
		bool active = false;
		bool controller = false;
	};

	static constexpr float headHeightBand = .05f;
	static constexpr float maxLength = 3.f;
	static constexpr uint32_t sides = 8, rings = 4;

	static bool IsController( std::string_view profile )
	{
		return profile == "/interaction_profiles/khr/simple_controller" || profile == "/interaction_profiles/oculus/touch_controller" || profile == "/interaction_profiles/htc/vive_controller" ||
			   profile == "/interaction_profiles/valve/index_controller" || profile == "/interaction_profiles/microsoft/motion_controller";
	}

	static bool Visible( const SSample &sample, const XrSpaceLocation &head, bool suppressPlatform )
	{
		const auto valid = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
		const auto tracked = XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
		if ( !sample.active || ( sample.flags & valid ) != valid )
			return false;

		if ( suppressPlatform || !sample.controller || ( sample.flags & tracked ) != tracked || !( head.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT ) )
			return false;

		return std::abs( sample.pose.position.y - head.pose.position.y ) > headHeightBand;
	}

	static float Length( const SSceneState &scene, XrPosef aim )
	{
		float length = maxLength;
		for ( float side : { -1.f, 1.f } )
		{
			float x, y;
			if ( !SPanelLayout::Intersect( scene, side, aim, x, y ) )
				continue;

			const auto panel = SPanelLayout::Pose( scene, side );
			const auto offset = SSceneState::Rotate( panel.orientation, { x, y, 0 } );
			const auto dx = panel.position.x + offset.x - aim.position.x;
			const auto dy = panel.position.y + offset.y - aim.position.y;
			const auto dz = panel.position.z + offset.z - aim.position.z;
			length = std::min( length, std::sqrt( dx * dx + dy * dy + dz * dz ) );
		}

		return length;
	}

	static void UpdateVertices( std::vector< xrlib::SMeshVertex > &vertices, float length )
	{
		const float distances[] = { 0, std::min( .3f, length * .45f ), length - std::min( .05f, length * .1f ), length };
		const float opacity[] = { 0, .65f, .65f, .1f };
		vertices.resize( rings * sides );
		for ( uint32_t ring = 0; ring < rings; ++ring )
			for ( uint32_t side = 0; side < sides; ++side )
			{
				const float angle = 2.f * std::numbers::pi_v< float > * side / sides;
				auto &vertex = vertices[ ring * sides + side ];
				vertex = {};
				vertex.position = { .001f * std::cos( angle ), .001f * std::sin( angle ), -distances[ ring ] };
				vertex.color0 = SPanelPalette::ray;
				vertex.uv0.x = opacity[ ring ];
			}
	}

	static void InitMesh( xrlib::CRenderModel &model )
	{
		UpdateVertices( model.vertices, maxLength );
		model.indices.clear();
		for ( uint32_t ring = 0; ring + 1 < rings; ++ring )
			for ( uint32_t side = 0; side < sides; ++side )
			{
				const uint32_t a = ring * sides + side, b = ring * sides + ( side + 1 ) % sides;
				for ( auto index : { a, a + sides, b, b, a + sides, b + sides } )
					model.indices.push_back( index );
			}
	}
};
