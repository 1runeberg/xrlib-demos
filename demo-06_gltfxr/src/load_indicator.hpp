// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include "panel_mesh.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

// Loading ring and label where the model will appear, drawn with the unlit panel pipeline
struct SLoadIndicator
{
	static constexpr float radius = .12f, thickness = .012f;
	static constexpr uint32_t trackSegments = 64, arcSegments = 48;
	static constexpr float spinSeconds = 1.5f, spinSweep = .25f; // Arc length in turns while progress is unknown

	struct SProgress
	{
		std::string name, stage;
		float fraction = -1.f; // Negative while the load has no measurable progress

		bool operator==( const SProgress & ) const = default;
	};

	static void Segment( std::vector< xrlib::SMeshVertex > &vertices, float from, float to, float alpha, float z )
	{
		const float inner = radius - thickness * .5f, outer = radius + thickness * .5f;
		for ( auto [ angle, distance ] : { std::pair { from, inner }, std::pair { to, inner }, std::pair { to, outer }, std::pair { from, outer } } )
		{
			xrlib::SMeshVertex vertex {};
			vertex.position = { distance * std::cos( angle ), distance * std::sin( angle ), z };
			vertex.color0 = SPanelPalette::accent;
			vertex.uv0.x = alpha;
			vertices.push_back( vertex );
		}
	}

	// A faint full track with the progress arc on top, always the same vertex count
	static void UpdateRing( std::vector< xrlib::SMeshVertex > &vertices, float fraction, float seconds )
	{
		constexpr float turn = 2.f * std::numbers::pi_v< float >;
		vertices.clear();
		for ( uint32_t i = 0; i < trackSegments; ++i )
			Segment( vertices, turn * i / trackSegments, turn * ( i + 1 ) / trackSegments, .2f, 0 );

		// Clockwise from the top, spinning while progress is unknown
		const float start = fraction < 0 ? std::fmod( seconds / spinSeconds, 1.f ) : 0.f;
		const float sweep = fraction < 0 ? spinSweep : std::clamp( fraction, 0.f, 1.f );
		for ( uint32_t i = 0; i < arcSegments; ++i )
		{
			const float from = start + sweep * i / arcSegments, to = start + sweep * ( i + 1 ) / arcSegments;
			Segment( vertices, turn * ( .25f - from ), turn * ( .25f - to ), 1.f, .001f );
		}
	}

	static void InitRing( xrlib::CRenderModel &model )
	{
		UpdateRing( model.vertices, 0, 0 );
		model.indices.clear();
		for ( uint32_t quad = 0; quad < trackSegments + arcSegments; ++quad )
			for ( uint32_t index : { 0u, 1u, 2u, 0u, 2u, 3u } )
				model.indices.push_back( quad * 4 + index );
	}

	// Model name and stage, centred under the ring
	static void UpdateLabel( xrlib::CRenderModel &model, const SProgress &progress )
	{
		SPanelMesh mesh( model, false );
		float top = -radius - .03f;
		for ( auto [ line, color ] : { std::pair { progress.name, SPanelPalette::text }, std::pair { progress.stage, SPanelPalette::accent } } )
		{
			const float width = stb_easy_font_width( line.data() ) * .0022f;
			mesh.Text( line, -width * .5f, top, width + .01f, color );
			top -= .03f;
		}
	}

	// Face the viewer around the vertical axis
	static XrPosef Pose( XrVector3f centre, XrVector3f head )
	{
		const float yaw = std::atan2( head.x - centre.x, head.z - centre.z );
		return { { 0, std::sin( yaw * .5f ), 0, std::cos( yaw * .5f ) }, centre };
	}
};
