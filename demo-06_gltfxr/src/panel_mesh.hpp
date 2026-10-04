// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include "panel_palette.hpp"
#include "panel_state.hpp"
#include <algorithm>
#include <cmath>
#include <stb/stb_easy_font.h>
#include <string>
#include <xrvk/mesh.hpp>

// Both hover states of each hoverable element, so a hover change swaps vertices instead of rebuilding the panel
struct SPanelHoverColors
{
	struct SSpan
	{
		int target;
		size_t first;
		std::vector< xrlib::SMeshVertex > normal, hovered;
	};

	std::vector< SSpan > spans;
	SPanelHover applied;

	void Apply( std::vector< xrlib::SMeshVertex > &outVertices, const SPanelHover &hover )
	{
		for ( const auto &span : spans )
		{
			const auto &source = hover.Contains( span.target ) ? span.hovered : span.normal;
			std::copy( source.begin(), source.end(), outVertices.begin() + span.first );
		}

		applied = hover;
	}
};

// Unlit panel geometry, shared by the controls and console
struct SPanelMesh
{
	std::vector< xrlib::SMeshVertex > &vertices;
	std::vector< uint32_t > &indices;
	SPanelHoverColors *pHover = nullptr;

	explicit SPanelMesh( xrlib::CRenderModel &outModel, bool background = true )
		: SPanelMesh( outModel.vertices, outModel.indices, background )
	{
	}

	// Records hoverable elements so hover can recolour the panel in place
	SPanelMesh( xrlib::CRenderModel &outModel, SPanelHoverColors &outHover )
		: SPanelMesh( outModel )
	{
		outHover.spans.clear();
		pHover = &outHover;
	}

	// Builds into caller-owned geometry, such as a fixed-size mesh updated in place
	SPanelMesh( std::vector< xrlib::SMeshVertex > &outVertices, std::vector< uint32_t > &outIndices, bool background )
		: vertices( outVertices )
		, indices( outIndices )
	{
		vertices.clear();
		indices.clear();
		if ( background )
			Quad( -.45f, -.25f, .45f, .25f, SPanelPalette::background, .85f );
	}

	void Quad( float left, float bottom, float right, float top, XrVector3f color, float alpha = 1.f, float z = 0 )
	{
		const uint32_t base = static_cast< uint32_t >( vertices.size() );
		for ( auto position : { XrVector3f { left, bottom, z }, XrVector3f { right, bottom, z }, XrVector3f { right, top, z }, XrVector3f { left, top, z } } )
		{
			xrlib::SMeshVertex vertex {};
			vertex.position = position;
			vertex.color0 = color;
			vertex.uv0.x = alpha;
			vertices.push_back( vertex );
		}

		for ( auto index : { 0u, 1u, 2u, 0u, 2u, 3u } )
			indices.push_back( base + index );
	}

	// Builds an element unhovered, keeping its hovered vertices for SPanelHoverColors. Both states need the same geometry
	template < typename Build > void Hoverable( int target, Build build )
	{
		const size_t first = vertices.size(), firstIndex = indices.size();
		if ( !pHover )
		{
			build( false );
			return;
		}

		build( true );
		std::vector< xrlib::SMeshVertex > hovered( vertices.begin() + first, vertices.end() );
		vertices.resize( first );
		indices.resize( firstIndex );

		build( false );
		if ( vertices.size() - first == hovered.size() )
			pHover->spans.push_back( { target, first, { vertices.begin() + first, vertices.end() }, std::move( hovered ) } );
	}

	void Text( std::string text, float x, float y, float maxWidth = .84f, XrVector3f color = SPanelPalette::text )
	{
		struct SFontVertex
		{
			float x, y, z;
			unsigned char color[ 4 ];
		};

		// Reused across rebuilds, stb only reads back the quads it wrote
		static thread_local std::vector< SFontVertex > glyphs;
		if ( glyphs.size() < text.size() * 128 )
			glyphs.resize( text.size() * 128 );

		const int quads = stb_easy_font_print( 0, 0, text.data(), nullptr, glyphs.data(), static_cast< int >( glyphs.size() * sizeof( SFontVertex ) ) );
		for ( int i = 0; i < quads; ++i )
		{
			float left = INFINITY, right = -INFINITY, top = -INFINITY, bottom = INFINITY;
			for ( int corner = 0; corner < 4; ++corner )
			{
				const auto &vertex = glyphs[ i * 4 + corner ];
				left = std::min( left, x + vertex.x * .0022f );
				right = std::max( right, x + vertex.x * .0022f );
				top = std::max( top, y - vertex.y * .0022f );
				bottom = std::min( bottom, y - vertex.y * .0022f );
			}

			if ( left < x + maxWidth )
				Quad( left, bottom, std::min( right, x + maxWidth ), top, color, 1, .002f );
		}
	}

	// A hidden outline keeps the geometry stable for hover
	void Outline( float left, float bottom, float right, float top, XrVector3f color = SPanelPalette::accent, bool visible = true )
	{
		const float line = .0025f, alpha = visible ? 1.f : 0.f;
		Quad( left, bottom, right, bottom + line, color, alpha, .0015f );
		Quad( left, top - line, right, top, color, alpha, .0015f );
		Quad( left, bottom, left + line, top, color, alpha, .0015f );
		Quad( right - line, bottom, right, top, color, alpha, .0015f );
	}

	void DashedOutline( float left, float bottom, float right, float top )
	{
		const auto color = SPanelPalette::disabledOutline;
		const float line = .0015f, dash = .004f, gap = .003f;

		for ( float x = left; x < right; x += dash + gap )
		{
			const float end = std::min( x + dash, right );
			Quad( x, bottom, end, bottom + line, color, 1, .0015f );
			Quad( x, top - line, end, top, color, 1, .0015f );
		}

		for ( float y = bottom + line; y < top - line; y += dash + gap )
		{
			const float end = std::min( y + dash, top - line );
			Quad( left, y, left + line, end, color, 1, .0015f );
			Quad( right - line, y, right, end, color, 1, .0015f );
		}
	}

	void ButtonSurface( float left, float bottom, float right, float top, bool enabled, bool hovered )
	{
		if ( !enabled )
		{
			DashedOutline( left, bottom, right, top );
			return;
		}

		Quad( left, bottom, right, top, hovered ? SPanelPalette::hover : SPanelPalette::button, 1, .001f );
		Outline( left, bottom, right, top, SPanelPalette::buttonOutline );
	}

	void ScrollArrow( bool up, float right, bool enabled, bool hovered, float bottom, float top )
	{
		const float left = SPanelLayout::ArrowLeft( right, up );
		const float edge = left + SPanelLayout::arrowWidth;
		const XrVector3f color = enabled ? ( hovered ? SPanelPalette::text : SPanelPalette::accent ) : SPanelPalette::disabledButtonText;
		ButtonSurface( left, bottom, edge, top, enabled, hovered );

		const float centreX = ( left + edge ) * .5f, centreY = ( bottom + top ) * .5f;
		const float direction = up ? 1.f : -1.f;
		const uint32_t base = static_cast< uint32_t >( vertices.size() );
		for ( auto point : { XrVector2f { -.011f, -.004f }, XrVector2f { 0, .007f }, XrVector2f { .011f, -.004f }, XrVector2f { .011f, -.007f }, XrVector2f { 0, .004f }, XrVector2f { -.011f, -.007f } } )
		{
			xrlib::SMeshVertex vertex {};
			vertex.position = { centreX + point.x, centreY + direction * point.y, .002f };
			vertex.color0 = color;
			vertex.uv0.x = 1;
			vertices.push_back( vertex );
		}

		for ( auto index : { 0u, 1u, 4u, 0u, 4u, 5u, 1u, 2u, 3u, 1u, 3u, 4u } )
			indices.push_back( base + index );
	}

	void PlaybackButton( bool playing, bool enabled, bool hovered )
	{
		const float left = SPanelLayout::playbackLeft, right = SPanelLayout::playbackRight;
		const float bottom = SPanelLayout::listButtonBottom, top = SPanelLayout::listButtonTop;
		const float centreX = ( left + right ) * .5f, centreY = ( bottom + top ) * .5f;
		const XrVector3f color = enabled ? ( hovered ? SPanelPalette::text : SPanelPalette::accent ) : SPanelPalette::disabledButtonText;
		ButtonSurface( left, bottom, right, top, enabled, hovered );

		if ( playing )
			Quad( centreX - .007f, centreY - .007f, centreX + .007f, centreY + .007f, color, 1, .002f );
		else
		{
			const uint32_t base = static_cast< uint32_t >( vertices.size() );
			for ( auto point : { XrVector2f { -.006f, -.009f }, XrVector2f { .01f, 0 }, XrVector2f { -.006f, .009f } } )
			{
				xrlib::SMeshVertex vertex {};
				vertex.position = { centreX + point.x, centreY + point.y, .002f };
				vertex.color0 = color;
				vertex.uv0.x = 1;
				vertices.push_back( vertex );
			}

			for ( auto index : { 0u, 1u, 2u } )
				indices.push_back( base + index );
		}
	}

	// Straight line of the given width, for icons
	void Line( float x0, float y0, float x1, float y1, float width, XrVector3f color )
	{
		const float dx = x1 - x0, dy = y1 - y0, length = std::max( std::sqrt( dx * dx + dy * dy ), 1e-6f );
		const float nx = -dy / length * width * .5f, ny = dx / length * width * .5f;
		const uint32_t base = static_cast< uint32_t >( vertices.size() );
		for ( auto position : { XrVector3f { x0 - nx, y0 - ny, .002f }, XrVector3f { x1 - nx, y1 - ny, .002f }, XrVector3f { x1 + nx, y1 + ny, .002f }, XrVector3f { x0 + nx, y0 + ny, .002f } } )
		{
			xrlib::SMeshVertex vertex {};
			vertex.position = position;
			vertex.color0 = color;
			vertex.uv0.x = 1;
			vertices.push_back( vertex );
		}

		for ( auto index : { 0u, 1u, 2u, 0u, 2u, 3u } )
			indices.push_back( base + index );
	}

	// Console panel view toggle, showing the view it switches to: a line graph or console lines
	void ViewButton( bool toGraph, bool hovered )
	{
		const float left = SPanelLayout::statsLeft, right = SPanelLayout::statsRight, bottom = -.225f, top = -.175f;
		const XrVector3f color = hovered ? SPanelPalette::text : SPanelPalette::accent;
		ButtonSurface( left, bottom, right, top, true, hovered );

		const float centreX = ( left + right ) * .5f, base = bottom + .013f;
		if ( toGraph )
		{

			// Axes with a rising zigzag
			Quad( centreX - .015f, base, centreX - .0125f, base + .025f, color, 1, .002f );
			Quad( centreX - .015f, base, centreX + .016f, base + .0025f, color, 1, .002f );
			const XrVector2f points[] = { { -.011f, .006f }, { -.004f, .016f }, { .003f, .009f }, { .014f, .022f } };
			for ( int i = 0; i < 3; ++i )
				Line( centreX + points[ i ].x, base + points[ i ].y, centreX + points[ i + 1 ].x, base + points[ i + 1 ].y, .003f, color );

			return;
		}

		for ( auto [ row, width ] : { std::pair { 0, .026f }, std::pair { 1, .018f }, std::pair { 2, .022f } } )
		{
			const float y = top - .014f - row * .009f;
			Quad( centreX - .013f, y - .0025f, centreX - .013f + width, y + .0025f, color, 1, .002f );
		}
	}

	// Text ending at the given x
	void TextRight( const std::string &text, float rightX, float y, XrVector3f color = SPanelPalette::text )
	{
		std::string copy = text;
		const float width = stb_easy_font_width( copy.data() ) * .0022f;
		Text( text, rightX - width, y, width + .01f, color );
	}

	void Button( const std::string &label, float left, float right, bool enabled = true, bool hovered = false, float bottom = -.225f, float top = -.175f )
	{
		ButtonSurface( left, bottom, right, top, enabled, hovered );
		Text( label, left + .015f, top - .015f, right - left - .03f, enabled ? ( hovered ? SPanelPalette::text : SPanelPalette::accent ) : SPanelPalette::disabledButtonText );
	}
};
