// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <initializer_list>
#include <xrvk/mesh.hpp>
#include <xrvk/night_grid.hpp>

// Meshes for the xrvk night grid sky and floor shaders
struct SBackdrop
{
	static constexpr float skySize = 50.f;	 // Half extent in metres, corners stay inside the far plane
	static constexpr float floorSize = 4.5f; // Half extent in metres, matches --floor-size in the asset build
	static constexpr float starPixel = .001f;  // Radians, a bit over an eye buffer pixel so quads fit the filtered glow

	static void Quad( xrlib::CRenderModel &model, std::initializer_list< XrVector3f > corners )
	{
		const uint32_t base = static_cast< uint32_t >( model.vertices.size() );
		for ( const auto &corner : corners )
		{
			xrlib::SMeshVertex vertex {};
			vertex.position = corner;
			model.vertices.push_back( vertex );
		}

		for ( uint32_t index : { 0u, 1u, 2u, 0u, 2u, 3u } )
			model.indices.push_back( base + index );
	}

	// Culling is off for both meshes, so winding doesn't matter
	static void Finish( xrlib::CRenderModel &model, xrlib::SMaterial material )
	{
		model.materials = { material };
		model.materialSections = { { 0, static_cast< uint32_t >( model.indices.size() ), 0 } };
	}

	// Head-centred cube, the base colour texture is the baked night sky
	static void InitSky( xrlib::CRenderModel &model, int texture )
	{
		model.vertices.clear();
		model.indices.clear();
		for ( float x : { -1.f, 1.f } )
			for ( float y : { -1.f, 1.f } )
				for ( float z : { -1.f, 1.f } )
				{
					xrlib::SMeshVertex vertex {};
					vertex.position = { x, y, z };
					model.vertices.push_back( vertex );
				}

		model.indices = { 0, 1, 3, 0, 3, 2, 4, 6, 7, 4, 7, 5, 0, 4, 5, 0, 5, 1, 2, 3, 7, 2, 7, 6, 0, 2, 6, 0, 6, 4, 1, 5, 7, 1, 7, 3 };

		xrlib::SMaterial material;
		material.baseColorTexture = texture;
		Finish( model, material );
		model.instances[ 0 ].scale = { skySize, skySize, skySize };
	}

	// Unit-radius star quads, scaled to the sky and expanded by the star shader
	static void InitStars( xrlib::CRenderModel &model )
	{
		model.vertices.clear();
		model.indices.clear();
		for ( const auto &star : xrlib::NightGridStars() )
		{
			const uint32_t base = static_cast< uint32_t >( model.vertices.size() );
			const float extent = 2.5f * std::max( star.size, starPixel );
			for ( XrVector2f corner : { XrVector2f { -1, -1 }, XrVector2f { 1, -1 }, XrVector2f { 1, 1 }, XrVector2f { -1, 1 } } )
			{
				xrlib::SMeshVertex vertex {};
				vertex.position = { star.direction[ 0 ], star.direction[ 1 ], star.direction[ 2 ] };
				vertex.normal = { 1, 0, 0 };
				vertex.uv0 = corner;
				vertex.uv1 = { star.size, extent };
				vertex.color0 = { star.radiance[ 0 ], star.radiance[ 1 ], star.radiance[ 2 ] };
				model.vertices.push_back( vertex );
			}

			for ( uint32_t index : { 0u, 1u, 2u, 0u, 2u, 3u } )
				model.indices.push_back( base + index );
		}

		Finish( model, {} );
		model.instances[ 0 ].scale = { skySize, skySize, skySize };
	}

	// Floor in metres on the XZ plane, the shader draws the grid in these coordinates
	static void InitFloor( xrlib::CRenderModel &model )
	{
		model.vertices.clear();
		model.indices.clear();
		Quad( model, { { -floorSize, 0, -floorSize }, { floorSize, 0, -floorSize }, { floorSize, 0, floorSize }, { -floorSize, 0, floorSize } } );

		// The shader scales the grid fade from this, the untextured material also keeps set 0 bound
		xrlib::SMaterial material;
		material.emissiveFactor[ 0 ] = floorSize;
		Finish( model, material );
	}
};
