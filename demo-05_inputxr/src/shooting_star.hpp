// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include "backdrop.hpp"
#include <xrlib/common.hpp>
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

// An occasional meteor in the night sky ahead of the viewer, drawn with the star shader as a
// fading trail quad and a glowing head quad
struct SShootingStar
{
	static constexpr float minGap = 4.f, maxGap = 10.f; // Seconds between streaks
	static constexpr float minDuration = .6f, maxDuration = 1.f;
	static constexpr float tailAngle = .15f;			// Radians of trail behind the head
	static constexpr float trailSize = .0012f, headSize = .002f; // Angular radii
	static constexpr XrVector3f radiance { 3.f, 3.2f, 3.6f };

	std::mt19937 random { std::random_device {}() };
	bool active = false;
	float start = 0, duration = 0, arc = 0;
	XrVector3f from {}, tangent {};

	static XrVector3f Scale( XrVector3f v, float s ) { return { v.x * s, v.y * s, v.z * s }; }
	static XrVector3f Add( XrVector3f a, XrVector3f b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	static float Dot( XrVector3f a, XrVector3f b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	static XrVector3f Cross( XrVector3f a, XrVector3f b ) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
	static XrVector3f Normalise( XrVector3f v ) { return Scale( v, 1.f / std::sqrt( std::max( Dot( v, v ), 1e-12f ) ) ); }
	static float Smoothstep( float edge0, float edge1, float x )
	{
		const float t = std::clamp( ( x - edge0 ) / ( edge1 - edge0 ), 0.f, 1.f );
		return t * t * ( 3.f - 2.f * t );
	}

	float Uniform( float low, float high ) { return std::uniform_real_distribution< float >( low, high )( random ); }

	// Point on the streak's great circle, angle radians from its start
	XrVector3f Along( float angle ) const { return Add( Scale( from, std::cos( angle ) ), Scale( tangent, std::sin( angle ) ) ); }

	void Init( xrlib::CRenderModel &model, float seconds )
	{
		model.vertices.assign( 8, {} );
		model.indices = { 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7 };
		SBackdrop::Finish( model, {} );
		model.instances[ 0 ].scale = { SBackdrop::skySize, SBackdrop::skySize, SBackdrop::skySize };
		start = seconds + Uniform( minGap, maxGap );
	}

	// Picks a descending path in front of where the viewer is facing
	void Begin( const XrQuaternionf &head )
	{
		XrVector3f forward;
		const XrVector3f ahead { 0, 0, -1 };
		XrQuaternionf_RotateVector3f( &forward, &head, &ahead );

		forward.y = 0;
		forward = Dot( forward, forward ) > 1e-6f ? Normalise( forward ) : XrVector3f { 0, 0, -1 };
		const XrVector3f right { -forward.z, 0, forward.x };

		auto Direction = [ & ]( float azimuth, float elevation )
		{
			const XrVector3f horizontal = Add( Scale( forward, std::cos( azimuth ) ), Scale( right, std::sin( azimuth ) ) );
			return Add( Scale( horizontal, std::cos( elevation ) ), { 0, std::sin( elevation ), 0 } );
		};

		const float azimuth = Uniform( -.9f, .9f ), elevation = Uniform( .45f, .95f );
		const float sweep = Uniform( .25f, .4f ) * ( Uniform( 0, 1 ) < .5f ? -1.f : 1.f );
		from = Direction( azimuth, elevation );
		const XrVector3f to = Direction( azimuth + sweep, elevation - Uniform( .1f, .25f ) );

		const float cosine = std::clamp( Dot( from, to ), -1.f, 1.f );
		arc = std::acos( cosine );
		tangent = Normalise( Add( to, Scale( from, -cosine ) ) );
		duration = Uniform( minDuration, maxDuration );
	}

	// Rebuilds the quads, returns false while no streak is showing
	bool Update( std::vector< xrlib::SMeshVertex > &vertices, float seconds, const XrQuaternionf &head )
	{
		if ( !active )
		{
			if ( seconds < start )
				return false;

			Begin( head );
			active = true;
		}

		const float t = ( seconds - start ) / duration;
		if ( t >= 1.f )
		{
			active = false;
			start = seconds + Uniform( minGap, maxGap );
			return false;
		}

		const float fade = Smoothstep( 0.f, .15f, t ) * ( 1.f - Smoothstep( .6f, 1.f, t ) );
		const float headAngle = arc * t, tail = std::max( 0.f, headAngle - tailAngle );
		const XrVector3f headPoint = Along( headAngle ), tailPoint = Along( tail );
		const XrVector3f direction = Add( Scale( from, -std::sin( headAngle ) ), Scale( tangent, std::cos( headAngle ) ) );
		const XrVector3f side = Normalise( Cross( headPoint, direction ) );

		// The trail brightens from nothing at the tail, with corners already placed across its width
		const float trailExtent = 2.5f * std::max( trailSize, SBackdrop::starPixel );
		const XrVector3f trailColor = Scale( radiance, .6f * fade );
		for ( uint32_t i = 0; i < 4; ++i )
		{
			const bool atHead = i == 1 || i == 2;
			const float across = i < 2 ? -1.f : 1.f;
			auto &vertex = vertices[ i ];
			vertex = {};
			vertex.position = Add( atHead ? headPoint : tailPoint, Scale( side, across * trailExtent ) );
			vertex.uv0 = { across, 0 };
			vertex.uv1 = { trailSize, trailExtent };
			vertex.color0 = atHead ? trailColor : XrVector3f {};
		}

		// The head glow expands in the shader like a star
		const float headExtent = 2.5f * std::max( headSize, SBackdrop::starPixel );
		const XrVector2f corners[] { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
		for ( uint32_t i = 0; i < 4; ++i )
		{
			auto &vertex = vertices[ 4 + i ];
			vertex = {};
			vertex.position = headPoint;
			vertex.normal = { 1, 0, 0 };
			vertex.uv0 = corners[ i ];
			vertex.uv1 = { headSize, headExtent };
			vertex.color0 = Scale( radiance, fade );
		}

		return true;
	}
};
