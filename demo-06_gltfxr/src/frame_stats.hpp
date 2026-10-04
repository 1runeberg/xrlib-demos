// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

// Rolling per-frame timing from portable OpenXR frame state and CPU clocks, in milliseconds
struct SFrameStats
{
	static constexpr double windowSeconds = 5.0;
	static constexpr uint32_t maxWindowFrames = 600; // Five seconds at 120 Hz
	static constexpr uint32_t capacity = 640;		 // The window plus a partly filled graph column
	static constexpr uint32_t columns = 150;		 // Graph columns, each averaging a whole number of frames
	static constexpr double missedFactor = 1.5;		 // Longer intervals than this many display periods repeated a frame
	static constexpr double gapMs = 250.0;			 // Longer intervals are pauses, which break the graph instead

	// Health targets, short spikes show as missed frame ticks without failing the averages
	static constexpr double missedTolerance = .01; // Missed frames as a fraction of the window
	static constexpr double rateTolerance = 1.05;  // Average frame interval relative to the display period
	static constexpr double cpuTarget = .8;		   // Average CPU time as a fraction of the display period
	static constexpr double headroomTarget = .1;   // Average wait time as a fraction of the display period

	struct SSample
	{
		float interval = 0, cpu = 0, wait = 0, period = 0;
		bool missed = false, valid = false;
	};

	struct SSummary
	{
		double periodMs = 0, targetMs = 0;
		double intervalAverage = 0, intervalMax = 0;
		double cpuAverage = 0, cpuMax = 0;
		double waitAverage = 0, waitMin = 0;
		uint32_t frames = 0, missed = 0;
		uint64_t missedTotal = 0;
	};

	std::vector< SSample > samples = std::vector< SSample >( capacity );
	uint64_t frames = 0;
	double periodMs = 0;					// Most common reported period, some runtimes report other periods around late frames
	double targetMs = 0;					// Fastest period the runtime has settled on, kept when it drops the rate under load
	std::map< int, uint32_t > periodCounts; // Recorded frames per reported period, in hundredths of a millisecond
	uint64_t missedTotal = 0;

	// Interval is between predicted display times, wait covers xrWaitFrame and CPU is the rest of the frame loop
	void Add( double displayPeriodMs, double intervalMs, double cpuMs, double waitMs )
	{

		// Swap the overwritten frame's period for this one, then take the most common, preferring shorter on ties
		const auto Key = []( double period ) { return static_cast< int >( std::lround( period * 100. ) ); };
		if ( frames >= capacity && samples[ frames % capacity ].period > 0 )
		{
			const auto evicted = periodCounts.find( Key( samples[ frames % capacity ].period ) );
			if ( evicted != periodCounts.end() && --evicted->second == 0 )
				periodCounts.erase( evicted );
		}

		if ( displayPeriodMs > 0 )
			++periodCounts[ Key( displayPeriodMs ) ];

		uint32_t most = 0;
		periodMs = 0;
		for ( const auto &[ key, count ] : periodCounts )
			if ( count > most )
			{
				most = count;
				periodMs = key / 100.;
			}

		// Wait for a settled rate before it can become the target
		if ( periodMs > 0 && most >= columns && ( targetMs <= 0 || periodMs < targetMs ) )
			targetMs = periodMs;

		SSample sample;
		sample.period = static_cast< float >( displayPeriodMs );
		if ( intervalMs > 0 && intervalMs <= gapMs )
		{
			sample = { static_cast< float >( intervalMs ), static_cast< float >( cpuMs ), static_cast< float >( waitMs ), sample.period, periodMs > 0 && intervalMs > periodMs * missedFactor, true };
			missedTotal += sample.missed ? 1 : 0;
		}

		samples[ frames % capacity ] = sample;
		++frames;
	}

	// Frames covering the window at the current display rate
	uint32_t WindowFrames() const { return periodMs > 0 ? std::clamp( static_cast< uint32_t >( std::lround( windowSeconds * 1000. / periodMs ) ), columns, maxWindowFrames ) : columns; }

	uint32_t FramesPerColumn() const { return std::max( 1u, WindowFrames() / columns ); }

	// Frames outside the recorded range read as gaps
	SSample At( uint64_t frame ) const { return frame < frames && frames - frame <= capacity ? samples[ frame % capacity ] : SSample {}; }

	// Oldest first within the window, the last sample is the newest frame
	SSample Sample( uint32_t index ) const { return frames + index >= WindowFrames() ? At( frames + index - WindowFrames() ) : SSample {}; }

	// Averages over whole groups of frames, so columns only scroll when a group completes
	SSample Column( uint32_t index ) const
	{
		const uint32_t group = FramesPerColumn();
		const uint64_t end = frames - frames % group;
		const uint64_t back = uint64_t( columns - index ) * group;
		if ( end < back )
			return {};

		SSample column;
		uint32_t valid = 0;
		for ( uint64_t frame = end - back; frame < end - back + group; ++frame )
		{
			const auto sample = At( frame );
			column.missed = column.missed || sample.missed;
			if ( !sample.valid )
				continue;

			column.interval += sample.interval;
			column.cpu += sample.cpu;
			column.wait += sample.wait;
			++valid;
		}

		if ( valid )
		{
			column.interval /= valid;
			column.cpu /= valid;
			column.wait /= valid;
			column.valid = true;
		}

		return column;
	}

	SSummary Summary() const
	{
		SSummary summary { periodMs, targetMs > 0 ? targetMs : periodMs };
		summary.missedTotal = missedTotal;
		summary.waitMin = INFINITY;
		for ( uint32_t i = 0; i < WindowFrames(); ++i )
		{
			const auto sample = Sample( i );
			if ( !sample.valid )
				continue;

			++summary.frames;
			summary.missed += sample.missed ? 1 : 0;
			summary.intervalAverage += sample.interval;
			summary.cpuAverage += sample.cpu;
			summary.waitAverage += sample.wait;
			summary.intervalMax = std::max( summary.intervalMax, static_cast< double >( sample.interval ) );
			summary.cpuMax = std::max( summary.cpuMax, static_cast< double >( sample.cpu ) );
			summary.waitMin = std::min( summary.waitMin, static_cast< double >( sample.wait ) );
		}

		if ( !summary.frames )
		{
			summary.waitMin = 0;
			return summary;
		}

		summary.intervalAverage /= summary.frames;
		summary.cpuAverage /= summary.frames;
		summary.waitAverage /= summary.frames;
		return summary;
	}
};
