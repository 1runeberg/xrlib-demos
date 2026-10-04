// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include "scene_state.hpp"
#include <algorithm>
#include <cstddef>
#include <numbers>
#include <vector>

// Visible rows in a pinch-scrollable list
struct SPanelScroll
{
	int first = 0;
	float remainder = 0;

	void Clamp( size_t count, int visible ) { first = std::clamp( first, 0, std::max( 0, static_cast< int >( count ) - visible ) ); }

	bool Move( int rows, size_t count, int visible )
	{
		const int previous = first;
		first += rows;
		Clamp( count, visible );
		return first != previous;
	}

	bool Drag( float metres, float rowHeight, size_t count, int visible )
	{
		remainder += metres;
		const int rows = static_cast< int >( remainder / rowHeight );
		remainder -= rows * rowHeight;
		return Move( rows, count, visible );
	}
};

// A panel target shared by hit testing and hover rendering
struct SPanelRegion
{
	int side, target;
	float left, bottom, right, top;

	bool Contains( float x, float y ) const { return x >= left && x <= right && y >= bottom && y <= top; }
};

// Combined hover from the two OpenXR aim actions
struct SPanelHover
{
	int targets[ 2 ] { -1, -1 };

	bool Contains( int target ) const { return targets[ 0 ] == target || targets[ 1 ] == target; }
	bool operator==( const SPanelHover & ) const = default;

	bool Set( int hand, int target )
	{
		if ( targets[ hand ] == target )
			return false;

		targets[ hand ] = target;
		return true;
	}
};

// Geometry shared by rendering and OpenXR ray hit testing
struct SPanelLayout
{
	static constexpr float width = .9f, height = .5f, offset = 1.f;
	static constexpr float listTop = .135f, rowHeight = .04f;
	static constexpr int modelRows = 5, consoleRows = 11;
	static constexpr float consoleTop = .158f, consoleRowHeight = .023f;
	static constexpr int none = -1, scrollUp = -2, scrollDown = -3, animation = -4, exit = -5, scrollArea = -6;
	static constexpr int animationUp = -7, animationDown = -8, animationScrollArea = -9, info = -10, mode = -11, stats = -12;
	static constexpr float listLeft = -.425f, listRight = .425f, listGap = .015f;
	static constexpr float listButtonBottom = -.125f, listButtonTop = -.08f;
	static constexpr float arrowWidth = .055f, arrowGap = .01f;
	static constexpr float playbackLeft = listRight - 3 * arrowWidth - 2 * arrowGap;
	static constexpr float playbackRight = playbackLeft + arrowWidth;
	static constexpr float statsLeft = -.425f, statsRight = statsLeft + arrowWidth; // Console panel chart toggle

	static float ArrowLeft( float right, bool up ) { return right - ( up ? 2 * arrowWidth + arrowGap : arrowWidth ); }

	static int RowTarget( int index, bool animations ) { return index * 2 + ( animations ? 1 : 0 ); }
	static bool IsAnimationRow( int target ) { return target >= 0 && target % 2 != 0; }

	// Turn each panel inward around its pinned centre, left is -1 and right is 1
	static XrPosef Pose( const SSceneState &scene, float side )
	{
		const float halfAngle = -side * std::numbers::pi_v< float > / 12.f;
		const XrQuaternionf inward { 0, std::sin( halfAngle ), 0, std::cos( halfAngle ) };
		return { SSceneState::Multiply( scene.orientation, inward ), scene.PanelPosition( side * offset ) };
	}

	static bool Project( const SSceneState &scene, float side, XrPosef ray, float &outX, float &outY )
	{
		const auto panel = Pose( scene, side );
		const auto &orientation = panel.orientation;
		const XrQuaternionf inverse { -orientation.x, -orientation.y, -orientation.z, orientation.w };
		const auto origin = SSceneState::Rotate( inverse, { ray.position.x - panel.position.x, ray.position.y - panel.position.y, ray.position.z - panel.position.z } );
		const auto direction = SSceneState::Rotate( inverse, SSceneState::Rotate( ray.orientation, { 0, 0, -1 } ) );
		if ( !scene.placed || direction.z >= -.0001f || origin.z <= 0 )
			return false;

		const float distance = -origin.z / direction.z;
		outX = origin.x + distance * direction.x;
		outY = origin.y + distance * direction.y;
		return std::isfinite( outX ) && std::isfinite( outY );
	}

	static bool Intersect( const SSceneState &scene, float side, XrPosef ray, float &outX, float &outY ) { return Project( scene, side, ray, outX, outY ) && std::abs( outX ) <= width * .5f && std::abs( outY ) <= height * .5f; }

	static std::vector< SPanelRegion > Regions( int modelFirst, size_t modelCount, int animationFirst, size_t animationCount, int consoleFirst, size_t consoleCount, bool hasModel, bool canChangeMode, bool exiting )
	{
		std::vector< SPanelRegion > regions;
		if ( exiting )
			return regions;

		regions.push_back( { 1, stats, statsLeft, -.225f, statsRight, -.175f } );
		if ( consoleCount > static_cast< size_t >( consoleRows ) )
			regions.push_back( { 1, scrollArea, -.435f, consoleTop - consoleRows * consoleRowHeight, .435f, consoleTop } );
		if ( consoleFirst > 0 )
			regions.push_back( { 1, scrollUp, ArrowLeft( listRight, true ), -.225f, listRight - arrowWidth - arrowGap, -.175f } );
		if ( consoleFirst + consoleRows < static_cast< int >( consoleCount ) )
			regions.push_back( { 1, scrollDown, ArrowLeft( listRight, false ), -.225f, listRight, -.175f } );

		for ( bool animations : { false, true } )
		{
			const int first = animations ? animationFirst : modelFirst;
			const auto count = animations ? animationCount : modelCount;
			const float left = animations ? listGap : listLeft;
			const float right = animations ? listRight : -listGap;
			if ( count > static_cast< size_t >( modelRows ) )
				regions.push_back( { -1, animations ? animationScrollArea : scrollArea, left, listTop - modelRows * rowHeight, right, listTop } );
			if ( first > 0 && count > 0 )
				regions.push_back( { -1, animations ? animationUp : scrollUp, ArrowLeft( right, true ), listButtonBottom, right - arrowWidth - arrowGap, listButtonTop } );
			if ( first + modelRows < static_cast< int >( count ) )
				regions.push_back( { -1, animations ? animationDown : scrollDown, ArrowLeft( right, false ), listButtonBottom, right, listButtonTop } );

			for ( int row = 0; row < modelRows && first + row < static_cast< int >( count ); ++row )
			{
				const float top = listTop - row * rowHeight;
				regions.push_back( { -1, RowTarget( first + row, animations ), left, top - rowHeight + .004f, right, top } );
			}
		}

		if ( canChangeMode )
			regions.push_back( { -1, mode, -.205f, -.225f, -.005f, -.175f } );
		if ( hasModel )
			regions.push_back( { -1, info, .015f, -.225f, .22f, -.175f } );
		if ( animationCount > 0 )
			regions.push_back( { -1, animation, playbackLeft, listButtonBottom, playbackRight, listButtonTop } );
		regions.push_back( { -1, exit, .24f, -.225f, .425f, -.175f } );
		return regions;
	}

	static std::vector< SPanelRegion > HoverRegions( const std::vector< SPanelRegion > &regions )
	{
		std::vector< SPanelRegion > targets;
		for ( const auto &region : regions )
		{

			// Rows already provide hover and drag targets, avoid a competing whole-list target
			if ( region.side < 0 && ( region.target == scrollArea || region.target == animationScrollArea ) )
				continue;

			targets.push_back( region );
		}

		return targets;
	}

	static int Hit( const std::vector< SPanelRegion > &regions, int side, float x, float y )
	{
		for ( auto region = regions.rbegin(); region != regions.rend(); ++region )
			if ( region->side == side && region->Contains( x, y ) )
				return region->target;

		return none;
	}

	static int ListAt( int side, float x, float y )
	{
		if ( side > 0 )
			return y <= consoleTop && y >= consoleTop - consoleRows * consoleRowHeight && std::abs( x ) <= .435f ? scrollArea : none;

		if ( y > listTop || y < listTop - modelRows * rowHeight )
			return none;

		if ( x >= listLeft && x <= -listGap )
			return scrollArea;

		if ( x >= listGap && x <= listRight )
			return animationScrollArea;

		return none;
	}
};
