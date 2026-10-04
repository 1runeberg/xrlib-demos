// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <fastgltf/types.hpp>
#include <string>
#include <string_view>
#include <vector>

// Small owned summary retained after the parsed glTF asset is released
struct SGltfInfo
{
	std::vector< std::string > lines;

	void Capture( const fastgltf::Asset &asset, const std::string &name, size_t vertices, size_t triangles )
	{
		lines.clear();
		lines.push_back( "glTF info: " + name );
		auto field = [ this ]( const char *label, std::string_view value )
		{
			if ( !value.empty() )
				lines.push_back( std::string( label ) + ": " + std::string( value ) );
		};
		if ( asset.assetInfo )
		{
			field( "Version", asset.assetInfo->gltfVersion );
			field( "Minimum version", asset.assetInfo->minVersion );
			field( "Generator", asset.assetInfo->generator );
			field( "Copyright", asset.assetInfo->copyright );
		}

		size_t primitives = 0, joints = 0, morphMeshes = 0, morphTargets = 0;
		for ( const auto &mesh : asset.meshes )
		{
			primitives += mesh.primitives.size();
			if ( !mesh.primitives.empty() && !mesh.primitives.front().targets.empty() )
			{
				++morphMeshes;
				morphTargets += mesh.primitives.front().targets.size();
			}
		}
		for ( const auto &skin : asset.skins )
			joints += skin.joints.size();

		lines.push_back( "Scenes: " + std::to_string( asset.scenes.size() ) + " / nodes: " + std::to_string( asset.nodes.size() ) );
		lines.push_back( "Meshes: " + std::to_string( asset.meshes.size() ) + " / primitives: " + std::to_string( primitives ) );
		lines.push_back( "Rendered vertices: " + std::to_string( vertices ) + " / triangles: " + std::to_string( triangles ) );
		lines.push_back( "Materials: " + std::to_string( asset.materials.size() ) + " / textures: " + std::to_string( asset.textures.size() ) + " / images: " + std::to_string( asset.images.size() ) );
		lines.push_back( "Skins: " + std::to_string( asset.skins.size() ) + " / joint entries: " + std::to_string( joints ) );
		lines.push_back( "Morph meshes: " + std::to_string( morphMeshes ) + " / targets: " + std::to_string( morphTargets ) );
		lines.push_back( "Animations: " + std::to_string( asset.animations.size() ) );
		for ( size_t index = 0; index < asset.animations.size(); ++index )
		{
			const auto &animation = asset.animations[ index ];
			const auto label = animation.name.empty() ? "Animation " + std::to_string( index + 1 ) : std::string( animation.name );
			lines.push_back( label + " / channels: " + std::to_string( animation.channels.size() ) );
		}

		for ( const auto &extension : asset.extensionsUsed )
			field( "Extension used", extension );
		for ( const auto &extension : asset.extensionsRequired )
			field( "Extension required", extension );
	}
};
