// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include "frame_stats.hpp"
#include "panel_mesh.hpp"
#include <array>
#include <cmath>
#include <cstdio>

// Frame timing view of the console panel. Labels are static, and the graph and numbers share one
// fixed-size mesh that updates in place, so the view doesn't reallocate buffers while it runs
struct SPerformancePanel
{
	static constexpr float left = -.425f, right = .425f, bottom = -.065f, top = .165f;
	static constexpr float lineWidth = .003f, markerHeight = .014f, z = .003f;
	static constexpr uint32_t textQuads = 1024; // Fixed room for the numbers, longer text is cut off

	// Numbers refreshed a few times a second, kept steady between graph updates
	struct SReadout
	{
		double targetMs = 0;
		std::string rate, missed, frame, wait, cpu;
		XrVector3f rateColor {}, missedColor {}, frameColor {}, waitColor {}, cpuColor {};
		std::vector< xrlib::SMeshVertex > glyphs; // Built once per refresh, then copied into each graph update
	};

	// Values map from zero to twice the display period, the budget sits halfway up
	static float GraphY( double milliseconds, double periodMs ) { return bottom + ( top - bottom ) * static_cast< float >( std::clamp( periodMs > 0 ? milliseconds / ( 2 * periodMs ) : 0., 0., 1. ) ); }

	// Columns of whole frame groups, the newest on the right
	static float GraphX( uint32_t column ) { return left + ( right - left ) * ( column + .5f ) / SFrameStats::columns; }

	static std::string Format( const char *format, double a, double b = 0 )
	{
		char text[ 96 ];
		std::snprintf( text, sizeof( text ), format, a, b );
		return text;
	}

	static float Width( std::string text ) { return stb_easy_font_width( text.data() ) * .0022f; }

	// Legend rows share these positions between the static labels and the updating values
	struct SLegend
	{
		float x, y;
		const char *label;
		XrVector3f swatch;
	};

	static constexpr float rateY = .193f;
	static constexpr SLegend frameLegend { -.425f, -.08f, "Frame", SPanelPalette::accent };
	static constexpr SLegend waitLegend { 0.f, -.08f, "Wait", SPanelPalette::graphWait };
	static constexpr SLegend cpuLegend { -.425f, -.11f, "CPU", SPanelPalette::graphCpu };
	static constexpr float missedLegendX = 0.f;

	static float ValueX( const SLegend &legend ) { return legend.x + .02f + Width( legend.label ) + .01f; }

	static SReadout Readout( const SFrameStats &stats )
	{
		const auto summary = stats.Summary();
		const auto Health = []( bool good ) { return good ? SPanelPalette::healthy : SPanelPalette::unhealthy; };

		// The rate follows the average interval, missed frames are judged separately beside it
		const bool fewMissed = summary.missed <= summary.frames * SFrameStats::missedTolerance;
		const bool onRate = summary.frames && summary.intervalAverage <= summary.targetMs * SFrameStats::rateTolerance;
		const double fps = summary.intervalAverage > 0 ? 1000. / summary.intervalAverage : 0.;

		SReadout readout { summary.targetMs };
		readout.rate = Format( "%.1f fps of %.0f Hz", fps, summary.targetMs > 0 ? 1000. / summary.targetMs : 0. );
		if ( summary.periodMs > summary.targetMs * SFrameStats::rateTolerance )
			readout.rate += Format( ", runtime at %.0f Hz", 1000. / summary.periodMs );

		readout.missed = "Missed " + std::to_string( summary.missed ) + " in 5 s, " + std::to_string( summary.missedTotal ) + " total";
		readout.frame = Format( "avg %.1f  max %.1f ms", summary.intervalAverage, summary.intervalMax );
		readout.wait = Format( "avg %.1f  min %.1f ms", summary.waitAverage, summary.waitMin );
		readout.cpu = Format( "avg %.1f  max %.1f ms", summary.cpuAverage, summary.cpuMax );

		// Frame should match the display period, CPU must fit inside it and wait is the headroom left over
		readout.rateColor = readout.frameColor = Health( onRate );
		readout.missedColor = Health( fewMissed );
		readout.waitColor = Health( summary.waitAverage >= summary.targetMs * SFrameStats::headroomTarget );
		readout.cpuColor = Health( summary.cpuAverage <= summary.targetMs * SFrameStats::cpuTarget );

		// Text quads use the same corner order as the graph, so the shared quad indices still apply
		std::vector< uint32_t > unused;
		SPanelMesh mesh( readout.glyphs, unused, false );
		mesh.Text( readout.rate, -.425f, rateY, .5f, readout.rateColor );
		mesh.Text( readout.missed, -.425f + Width( readout.rate ) + .04f, rateY, .5f, readout.missedColor );
		mesh.Text( readout.frame, ValueX( frameLegend ), frameLegend.y, .3f, readout.frameColor );
		mesh.Text( readout.wait, ValueX( waitLegend ), waitLegend.y, .3f, readout.waitColor );
		mesh.Text( readout.cpu, ValueX( cpuLegend ), cpuLegend.y, .3f, readout.cpuColor );

		readout.glyphs.resize( std::min< size_t >( readout.glyphs.size(), textQuads * 4 ) );
		for ( auto &vertex : readout.glyphs )
			vertex.position.z = z;

		return readout;
	}

	// Labels, budget line and targets, rebuilt when the target period changes
	static void UpdateMesh( xrlib::CRenderModel &outModel, double targetMs, SPanelHoverColors &outHover )
	{
		SPanelMesh mesh( outModel, outHover );
		mesh.Text( "PERFORMANCE MONITOR", -.425f, .225f );
		mesh.Quad( left, bottom, right, top, SPanelPalette::listBackground, 1, .001f );

		// Dashed budget line at one display period
		const float budget = GraphY( targetMs, targetMs );
		for ( float x = left; x < right; x += .02f )
			mesh.Quad( x, budget - .001f, std::min( x + .01f, right ), budget + .001f, SPanelPalette::graphBudget, 1, .0015f );

		mesh.Text( Format( "%.1f ms budget", targetMs ), left + .01f, budget - .008f, .3f, SPanelPalette::disabledText );
		mesh.Text( "2x", left + .01f, top - .005f, .1f, SPanelPalette::disabledText );

		for ( const auto &legend : { frameLegend, waitLegend, cpuLegend } )
		{
			mesh.Quad( legend.x, legend.y - .012f, legend.x + .012f, legend.y, legend.swatch, 1, .002f );
			mesh.Text( legend.label, legend.x + .02f, legend.y, .2f );
		}

		// The graph's top ticks, matching their thin vertical shape
		mesh.Quad( missedLegendX + .004f, cpuLegend.y - .014f, missedLegendX + .008f, cpuLegend.y, SPanelPalette::unhealthy, 1, .002f );
		mesh.Text( "Missed frame, repeated", missedLegendX + .02f, cpuLegend.y, .4f );

		// Averages over the window, apart from the missed frame share
		mesh.Text(
			"Targets  Frame <" + Format( "%.1f ms, <%.0f%% missed", targetMs * SFrameStats::rateTolerance, SFrameStats::missedTolerance * 100 ) + "   CPU <" + Format( "%.1f ms", targetMs * SFrameStats::cpuTarget ) + "   Wait >" +
				Format( "%.1f ms", targetMs * SFrameStats::headroomTarget ),
			-.425f,
			-.14f,
			.84f,
			SPanelPalette::disabledText );

		mesh.Hoverable( SPanelLayout::stats, [ & ]( bool hovered ) { mesh.ViewButton( false, hovered ); } );
		mesh.TextRight( "Last 5 s, averaged per column", SPanelLayout::listRight, -.19f );
	}

	static void Quad( std::vector< xrlib::SMeshVertex > &vertices, const XrVector2f ( &corners )[ 4 ], XrVector3f color, float alpha )
	{
		for ( const auto &corner : corners )
		{
			xrlib::SMeshVertex vertex {};
			vertex.position = { corner.x, corner.y, z };
			vertex.color0 = color;
			vertex.uv0.x = alpha;
			vertices.push_back( vertex );
		}
	}

	// Thin quad along a line, fully transparent when hidden so the vertex count stays fixed
	static void Segment( std::vector< xrlib::SMeshVertex > &vertices, float x0, float y0, float x1, float y1, XrVector3f color, bool visible )
	{
		const float dx = x1 - x0, dy = y1 - y0, length = std::max( std::sqrt( dx * dx + dy * dy ), 1e-6f );
		const float nx = -dy / length * lineWidth * .5f, ny = dx / length * lineWidth * .5f;
		Quad( vertices, { { x0 - nx, y0 - ny }, { x1 - nx, y1 - ny }, { x1 + nx, y1 + ny }, { x0 + nx, y0 + ny } }, color, visible ? 1.f : 0.f );
	}

	// Graph lines, missed frame ticks, then the numbers padded to a fixed size
	static void UpdateDynamic( std::vector< xrlib::SMeshVertex > &vertices, const SFrameStats &stats, const SReadout &readout )
	{
		vertices.clear();
		std::array< SFrameStats::SSample, SFrameStats::columns > columns;
		for ( uint32_t i = 0; i < SFrameStats::columns; ++i )
			columns[ i ] = stats.Column( i );

		// Scaled to the target rate, so a runtime rate drop shows as lines at the top
		const double target = stats.targetMs > 0 ? stats.targetMs : stats.periodMs;

		const struct
		{
			float SFrameStats::SSample::*value;
			XrVector3f color;
		} series[] = { { &SFrameStats::SSample::wait, SPanelPalette::graphWait }, { &SFrameStats::SSample::cpu, SPanelPalette::graphCpu }, { &SFrameStats::SSample::interval, SPanelPalette::accent } };

		for ( const auto &line : series )
			for ( uint32_t i = 0; i + 1 < SFrameStats::columns; ++i )
			{
				const auto &from = columns[ i ], &to = columns[ i + 1 ];
				Segment( vertices, GraphX( i ), GraphY( from.*line.value, target ), GraphX( i + 1 ), GraphY( to.*line.value, target ), line.color, from.valid && to.valid );
			}

		for ( uint32_t i = 0; i < SFrameStats::columns; ++i )
			Segment( vertices, GraphX( i ), top - markerHeight, GraphX( i ), top, SPanelPalette::unhealthy, columns[ i ].missed );

		vertices.insert( vertices.end(), readout.glyphs.begin(), readout.glyphs.end() );
		vertices.resize( vertices.size() + textQuads * 4 - readout.glyphs.size() );
	}

	static void InitDynamic( xrlib::CRenderModel &model )
	{
		UpdateDynamic( model.vertices, {}, {} );
		model.indices.clear();
		for ( uint32_t quad = 0; quad < model.vertices.size() / 4; ++quad )
			for ( uint32_t index : { 0u, 1u, 2u, 0u, 2u, 3u } )
				model.indices.push_back( quad * 4 + index );
	}
};
