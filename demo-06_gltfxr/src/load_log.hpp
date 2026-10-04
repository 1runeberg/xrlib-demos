// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include "panel_mesh.hpp"
#include <chrono>
#include <cstdio>

// Scrollable load history, following new messages when already at the bottom
struct SLoadLog
{
	std::vector< std::string > lines;
	std::string status = "Waiting for head tracking";
	SPanelScroll scroll;
	SPanelHover hover;
	bool dirty = true;
	bool performance = false; // The console panel shows the frame timing graph instead

	static double Elapsed( std::chrono::steady_clock::time_point start ) { return std::chrono::duration< double, std::milli >( std::chrono::steady_clock::now() - start ).count(); }

	void Add( const std::string &message )
	{
		fprintf( stderr, "GLTFXR_LOAD: %s\n", message.c_str() );
		const bool follow = scroll.first + SPanelLayout::consoleRows >= static_cast< int >( lines.size() );
		std::string line;
		for ( char character : message )
		{
			if ( character == '\n' || ( !line.empty() && stb_easy_font_width( ( line + character ).data() ) * .0022f > .83f ) )
			{
				lines.push_back( line );
				line.clear();
			}

			if ( character != '\n' )
				line += character;
		}

		if ( !line.empty() )
			lines.push_back( line );

		if ( follow )
			scroll.first = std::max( 0, static_cast< int >( lines.size() ) - SPanelLayout::consoleRows );

		dirty = true;
	}

	void Status( const std::string &message )
	{
		status = message;
		Add( message );
	}

	void Time( const char *stage, double milliseconds )
	{
		char text[ 128 ];
		std::snprintf( text, sizeof( text ), "%s: %.2f ms", stage, milliseconds );
		Add( text );
	}

	void UpdateMesh( xrlib::CRenderModel &outModel, SPanelHoverColors &outHover ) const
	{
		SPanelMesh mesh( outModel, outHover );
		mesh.Text( "CONSOLE", -.425f, .225f );
		mesh.Text( status, -.425f, .193f, .84f, SPanelPalette::accent );
		if ( lines.size() > static_cast< size_t >( SPanelLayout::consoleRows ) )
			mesh.Hoverable(
				SPanelLayout::scrollArea,
				[ & ]( bool hovered ) { mesh.Outline( -.435f, SPanelLayout::consoleTop - SPanelLayout::consoleRows * SPanelLayout::consoleRowHeight, .435f, SPanelLayout::consoleTop + .004f, SPanelPalette::accent, hovered ); } );

		for ( int row = 0; row < SPanelLayout::consoleRows && scroll.first + row < static_cast< int >( lines.size() ); ++row )
			mesh.Text( lines[ scroll.first + row ], -.425f, SPanelLayout::consoleTop - row * SPanelLayout::consoleRowHeight );

		mesh.Hoverable( SPanelLayout::scrollUp, [ & ]( bool hovered ) { mesh.ScrollArrow( true, SPanelLayout::listRight, scroll.first > 0, hovered, -.225f, -.175f ); } );
		mesh.Hoverable( SPanelLayout::scrollDown, [ & ]( bool hovered ) { mesh.ScrollArrow( false, SPanelLayout::listRight, scroll.first + SPanelLayout::consoleRows < static_cast< int >( lines.size() ), hovered, -.225f, -.175f ); } );
		mesh.Hoverable( SPanelLayout::stats, [ & ]( bool hovered ) { mesh.ViewButton( true, hovered ); } );
		mesh.TextRight( "Pinch + drag to scroll", SPanelLayout::ArrowLeft( SPanelLayout::listRight, true ) - .015f, -.19f );
	}
};
