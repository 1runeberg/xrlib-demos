/* Copyright 2026 Rune Berg. Licensed under Apache 2.0
 * SPDX-License-Identifier: Apache-2.0
 * gltfxr: static and animated PBR glTF models in passthrough
 */

#ifdef __APPLE__
	#define VK_ENABLE_BETA_EXTENSIONS
#endif

#include "../visionos/visionos_app_bridge.h"
#include "backdrop.hpp"
#include "gltf_info.hpp"
#include "load_indicator.hpp"
#include "load_log.hpp"
#include "model_retirement.hpp"
#include "panel_hover.hpp"
#include "panel_input.hpp"
#include "performance_panel.hpp"
#include "scene_state.hpp"
#include "shooting_star.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fastgltf/tools.hpp>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <xrlib.hpp>
#include <xrvk/animation.hpp>
#include <xrvk/environment.hpp>
#include <xrvk/gltf.hpp>
#include <xrvk/render.hpp>

#ifdef __APPLE__
	#include <TargetConditionals.h>
#endif

#ifdef XR_USE_PLATFORM_ANDROID
	#include <xrlib/ext/FB/passthrough.hpp>
#endif

using namespace xrlib;

namespace
{
	void Check( XrResult result )
	{
		if ( XR_FAILED( result ) )
			throw result;
	}

	void CheckVk( VkResult result, const char *operation = "Vulkan operation" )
	{
		if ( result != VK_SUCCESS )
			throw std::runtime_error( std::string( operation ) + " failed: VkResult " + std::to_string( result ) );
	}

	// Rewrite a panel's vertices in place, reallocating its buffers only when the layout changes
	void UploadPanel( CRenderModel &model, std::vector< uint32_t > &uploadedIndices )
	{
		if ( model.indices == uploadedIndices && model.UpdateVertexBuffer() == VK_SUCCESS )
			return;

		CheckVk( model.InitBuffers(), "Upload panel" );
		uploadedIndices = model.indices;
	}

	// Keep the renderer alive until scene cleanup, then wait for XR readers before
	// destroying the Vulkan device
	struct SRenderResources
	{
		CSession &session;
		std::unique_ptr< CStereoRender > renderer;
		std::unique_ptr< CRenderInfo > info;
		std::unique_ptr< CTextureManager > textures;
		std::vector< STexture > images;
		std::shared_ptr< CEnvironmentLighting > studio, night;
		CRenderModel *pModel = nullptr;

		~SRenderResources()
		{
			if ( !renderer )
				return;

			vkDeviceWaitIdle( session.GetVulkan()->GetVkLogicalDevice() );
#ifndef XR_USE_PLATFORM_ANDROID
			for ( auto chain : { renderer->GetColorSwapchain(), renderer->GetDepthSwapchain() } )
				if ( chain )
					for ( int i = 0; i < 3; ++i )
					{
						uint32_t index;
						if ( XR_FAILED( xrAcquireSwapchainImage( chain, nullptr, &index ) ) )
							break;

						XrSwapchainImageWaitInfo wait { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
						wait.timeout = XR_INFINITE_DURATION;
						if ( XR_FAILED( xrWaitSwapchainImage( chain, &wait ) ) )
							break;

						xrReleaseSwapchainImage( chain, nullptr );
					}
#endif

			// Wait for commands queued while draining swapchain images
			vkDeviceWaitIdle( session.GetVulkan()->GetVkLogicalDevice() );
			if ( textures && pModel )
				for ( auto &texture : pModel->textures )
					textures->DestroyTexture( texture );

			info.reset();
			if ( textures )
				for ( auto &image : images )
					textures->DestroyTexture( image );

			textures.reset();
		}
	};

	struct SModelEntry
	{
		std::string name, file, textures;
		float scale = 1.f;
		XrQuaternionf orientation { 0, 0, 0, 1 }; // Display orientation, applied before measuring the model's height
	};

	// Demo placement and playback (xrvk owns animation and skinning)
	struct SModelPlayback
	{
		XrVector3f centre {};
		float scale = 1.f;
		bool playing = false;
		XrTime epoch = 0;
		size_t clip = 0;

		void Update( CRenderModel &model, double seconds ) const
		{
			if ( model.pAnimation->GetClipCount() )
				model.pAnimation->Sample( seconds );
		}

		void Init( CRenderModel &model, XrQuaternionf orientation )
		{
			*this = {};
			if ( !model.pAnimation )
				throw std::runtime_error( "Model has no node hierarchy" );

			if ( model.pAnimation->GetClipCount() )
			{
				model.pAnimation->SetClip( size_t { 0 } );
				playing = true;
			}
			Update( model, 0 );
			model.pAnimation->Deform( model.vertices );

			XrVector3f minimum { INFINITY, INFINITY, INFINITY }, maximum { -INFINITY, -INFINITY, -INFINITY };
			for ( const auto &vertex : model.vertices )
			{
				const auto position = SSceneState::Rotate( orientation, { vertex.position.x, vertex.position.y, vertex.position.z } );
				minimum = { std::min( minimum.x, position.x ), std::min( minimum.y, position.y ), std::min( minimum.z, position.z ) };
				maximum = { std::max( maximum.x, position.x ), std::max( maximum.y, position.y ), std::max( maximum.z, position.z ) };
			}

			if ( !std::isfinite( maximum.y - minimum.y ) || maximum.y <= minimum.y )
				throw std::runtime_error( "Model has no valid height" );

			centre = { ( minimum.x + maximum.x ) * .5f, ( minimum.y + maximum.y ) * .5f, ( minimum.z + maximum.z ) * .5f };
			scale = SSceneState::size / ( maximum.y - minimum.y );
		}
	};

	// Own each load until its worker and GPU resources have been released
	struct SModelViewer
	{
		enum class EStage
		{
			Idle,
			Retire,
			Read,
			Upload,
			Bind
		};
		SRenderResources &resources;
		const SPipelines &pipelines;
		SLoadLog &log;
		std::vector< SModelEntry > models {
			{ "Damaged Helmet", "PreparedHelmet/DamagedHelmet.glb", "PreparedHelmet/textures" },
			{ "Fox", "PreparedFox/Fox.glb", "PreparedFox/textures", .8f },
			{ "BrainStem", "PreparedBrainStem/BrainStem.glb", "PreparedBrainStem/textures" },
			{ "Flight Helmet", "PreparedFlightHelmet/FlightHelmet.gltf", "PreparedFlightHelmet/textures" },

			// Palm towards the viewer with the fingers up
			{ "SteamVR Glove", "PreparedSteamVRGlove/steamvr_glove_l.glb", "PreparedSteamVRGlove/textures", .85f, { -.5f, .5f, .5f, .5f } },
			{ "Saber Hilt", "PreparedSaber/Saber.gltf", "PreparedSaber/textures" } };
		SPanelScroll scroll, animationScroll;
		SPanelHover hover;
		SModelPlayback playback;
		SGltfModel source;
		SGltfInfo modelInfo;
		std::unique_ptr< CGltf > loader;
		EStage stage = EStage::Idle;
		int pending = -1, selected = -1;
		bool ready = false, dirty = true, exiting = false;
		bool passthrough = true, canChangeMode = false;
		std::optional< uint32_t > pool;
		std::chrono::steady_clock::time_point started, readyAt;
		double setupMs = 0, prepareMs = 0, buffersMs = 0;
		VkCommandPool uploadPool = VK_NULL_HANDLE;
		std::vector< std::unique_ptr< vkutils::CImageUpload > > uploads;
		size_t uploadIndex = 0;
		bool uploadSubmitted = false;
		std::chrono::steady_clock::time_point uploadStarted;
		std::mutex progressMutex;
		std::vector< std::string > progress;
		std::future< bool > reading;
		std::unique_ptr< SModelRetirement > retirement;

		SModelViewer( SRenderResources &renderResources, const SPipelines &renderPipelines, SLoadLog &loadLog )
			: resources( renderResources )
			, pipelines( renderPipelines )
			, log( loadLog )
		{
			VkCommandPoolCreateInfo info { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
			info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
			info.queueFamilyIndex = resources.session.GetVulkan()->GetVkQueueIndex_GraphicsFamily();
			CheckVk( vkCreateCommandPool( resources.session.GetVulkan()->GetVkLogicalDevice(), &info, nullptr, &uploadPool ) );
		}

		void ClearSource()
		{
			SGltfModel empty;
			source = std::move( empty );
		}

		~SModelViewer()
		{
			if ( reading.valid() )
				reading.wait();

			Release();
			vkDestroyCommandPool( resources.session.GetVulkan()->GetVkLogicalDevice(), uploadPool, nullptr );
		}

		void Release()
		{
			retirement.reset();
			uploads.clear();
			uploadIndex = 0;
			uploadSubmitted = false;
			auto &info = *resources.info;
			if ( auto pModel = resources.pModel )
			{
				vkDeviceWaitIdle( resources.session.GetVulkan()->GetVkLogicalDevice() );
				std::erase( info.vecRenderables, pModel );
				for ( auto &texture : pModel->textures )
					resources.textures->DestroyTexture( texture );
				delete pModel;
				resources.pModel = nullptr;
			}

			if ( pool )
				info.pDescriptors->DeletePool( *pool );

			pool.reset();
			loader.reset();
			ClearSource();
			ready = false;
			playback = {};
			animationScroll = {};
			hover = {};
			modelInfo.lines.clear();
		}

		// Stage and progress for the indicator at the model position, empty when nothing is loading
		std::optional< SLoadIndicator::SProgress > LoadProgress() const
		{
			const int index = pending >= 0 ? pending : selected;
			if ( exiting || index < 0 || ( stage == EStage::Idle && pending < 0 ) )
				return std::nullopt;

			SLoadIndicator::SProgress progress { models[ index ].name, "Queued" };
			if ( stage == EStage::Retire )
				progress.stage = "Releasing previous model";
			else if ( stage == EStage::Read )
				progress.stage = log.status.starts_with( "Loading" ) ? "Loading" : log.status;
			else if ( stage == EStage::Upload && !uploads.empty() )
			{
				progress.stage = "Uploading textures " + std::to_string( std::min( uploadIndex + 1, uploads.size() ) ) + " / " + std::to_string( uploads.size() );
				progress.fraction = ( static_cast< float >( uploadIndex ) + ( uploadSubmitted ? .5f : 0.f ) ) / static_cast< float >( uploads.size() );
			}
			else if ( stage == EStage::Upload || stage == EStage::Bind )
			{
				progress.stage = "Binding materials";
				progress.fraction = 1.f;
			}

			return progress;
		}

		size_t AnimationCount() const { return ready ? resources.pModel->pAnimation->GetClipCount() : 0; }

		bool HasAnimation() const { return AnimationCount() > 0; }

		std::string AnimationName( size_t index ) const
		{
			const auto name = resources.pModel->pAnimation->GetClipName( index );
			return name.empty() ? "Animation " + std::to_string( index + 1 ) : std::string( name );
		}

		std::vector< SPanelRegion > Regions() const
		{
			return SPanelLayout::Regions( scroll.first, models.size(), animationScroll.first, AnimationCount(), log.performance ? 0 : log.scroll.first, log.performance ? 0 : log.lines.size(), ready, canChangeMode, exiting );
		}

		void SelectAnimation( size_t index )
		{
			if ( exiting || index >= AnimationCount() )
				return;

			try
			{
				resources.pModel->pAnimation->SetClip( index );
			}
			catch ( const std::exception &error )
			{
				log.Add( "Animation selection failed: " + std::string( error.what() ) );
				return;
			}

			playback.clip = index;
			playback.playing = true;
			playback.epoch = 0;
			playback.Update( *resources.pModel, 0 );
			CheckVk( resources.pModel->UpdateSkinning() );
			log.Add( "Animation playing: " + AnimationName( index ) );
			dirty = true;
		}

		void Hover( int hand, int side, int target )
		{
			hover.Set( hand, side < 0 && !exiting ? target : SPanelLayout::none );
			log.hover.Set( hand, side > 0 && !exiting ? target : SPanelLayout::none );
		}

		void Select( int index )
		{
			if ( exiting || index < 0 || index >= static_cast< int >( models.size() ) )
				return;

			pending = index;
			dirty = true;
			log.Add( "Selected: " + models[ index ].name + ( stage == EStage::Idle ? "" : " (queued)" ) );
		}

		void Click( int side, int target )
		{
			if ( exiting )
				return;

			const bool animations = side < 0 && ( target == SPanelLayout::animationUp || target == SPanelLayout::animationDown );
			auto &position = side > 0 ? log.scroll : animations ? animationScroll : scroll;
			const auto count = side > 0 ? log.lines.size() : animations ? AnimationCount() : models.size();
			const int rows = side > 0 ? SPanelLayout::consoleRows : SPanelLayout::modelRows;
			if ( target == SPanelLayout::scrollUp || target == SPanelLayout::scrollDown || animations )
			{
				const bool up = target == SPanelLayout::scrollUp || target == SPanelLayout::animationUp;
				position.Move( up ? -rows : rows, count, rows );
				dirty = log.dirty = true;
			}
			else if ( side < 0 && target >= 0 )
			{
				if ( SPanelLayout::IsAnimationRow( target ) )
					SelectAnimation( static_cast< size_t >( target / 2 ) );
				else
					Select( target / 2 );
			}
			else if ( side < 0 && target == SPanelLayout::animation && HasAnimation() )
			{
				playback.playing = !playback.playing;
				playback.epoch = 0;
				playback.Update( *resources.pModel, 0 );
				CheckVk( resources.pModel->UpdateSkinning() );
				log.Add( playback.playing ? "Animation playing: " + AnimationName( playback.clip ) : "Animation stopped" );
				dirty = true;
			}
			else if ( side < 0 && target == SPanelLayout::info && ready )
			{
				const int first = static_cast< int >( log.lines.size() );
				for ( const auto &line : modelInfo.lines )
					log.Add( line );
				log.scroll.first = first;
				log.scroll.Clamp( log.lines.size(), SPanelLayout::consoleRows );
				log.scroll.remainder = 0;
			}
			else if ( side > 0 && target == SPanelLayout::stats )
			{
				log.performance = !log.performance;
				log.dirty = true;
			}
			else if ( side < 0 && target == SPanelLayout::mode && canChangeMode )
			{
				passthrough = !passthrough;
				log.Add( passthrough ? "Mode: Passthrough" : "Mode: Immersive" );

				dirty = true;
			}
			else if ( side < 0 && target == SPanelLayout::exit )
			{
				exiting = true;
				pending = -1;
				if ( ready )
					playback.playing = false;

				log.Status( "Closing, waiting for pending work" );
				dirty = true;
			}
		}

		void Drag( int side, int list, float delta )
		{
			if ( exiting )
				return;

			const bool animations = side < 0 && list == SPanelLayout::animationScrollArea;
			auto &position = side > 0 ? log.scroll : animations ? animationScroll : scroll;
			const auto count = side > 0 ? log.lines.size() : animations ? AnimationCount() : models.size();
			const auto rowHeight = side > 0 ? SPanelLayout::consoleRowHeight : SPanelLayout::rowHeight;
			const auto rows = side > 0 ? SPanelLayout::consoleRows : SPanelLayout::modelRows;
			const bool moved = position.Drag( delta, rowHeight, count, rows );
			if ( side < 0 )
				dirty |= moved;
			else
				log.dirty |= moved;

		}

		void Progress( const std::string &message )
		{
			std::lock_guard guard( progressMutex );
			progress.push_back( message );
		}

		bool Prepare( CRenderModel *pModel )
		{
			if ( !loader->LoadFromDisk( pModel, &source, models[ selected ].file ) )
				return false;

			Progress( "Preparing textures and converting mesh" );
			auto preparation = std::chrono::steady_clock::now();
			loader->PrepareModel( pModel, &source );
			if ( pModel->vertices.empty() || pModel->materials.empty() )
				throw std::runtime_error( "Model has no renderable mesh or materials" );

			auto *pVulkan = resources.session.GetVulkan();
			for ( const auto &texture : pModel->textures )
			{
				if ( !texture.image || texture.data.empty() )
					continue;

				const vkutils::SImageUpload image { texture.image, texture.data, static_cast< uint32_t >( texture.width ), static_cast< uint32_t >( texture.height ), texture.format, texture.mips };
				auto upload = std::make_unique< vkutils::CImageUpload >();
				CheckVk( upload->Prepare( pVulkan->GetVkLogicalDevice(), pVulkan->GetVkPhysicalDevice(), uploadPool, { &image, 1 } ), "Prepare texture upload" );
				uploads.push_back( std::move( upload ) );
			}
			prepareMs = SLoadLog::Elapsed( preparation );

			Progress( "Preparing placement and mesh buffers" );
			auto setup = std::chrono::steady_clock::now();
			playback.Init( *pModel, models[ selected ].orientation );
			playback.scale *= models[ selected ].scale;
			setupMs = SLoadLog::Elapsed( setup );
			auto bufferStarted = std::chrono::steady_clock::now();
			XrMatrix4x4f modelFromAsset;
			const XrVector3f offset { -playback.centre.x * playback.scale, -playback.centre.y * playback.scale, -playback.centre.z * playback.scale };
			const XrVector3f scale { playback.scale, playback.scale, playback.scale };
			XrMatrix4x4f_CreateTranslationRotationScale( &modelFromAsset, &offset, &models[ selected ].orientation, &scale );
			CheckVk( pModel->InitSkinning( resources.info->pDescriptors->GetDescriptorSetLayout( pipelines.skinningDescriptorLayout ), modelFromAsset ), "Initialise GPU skinning" );
			CheckVk( pModel->InitBuffers() );
			buffersMs = SLoadLog::Elapsed( bufferStarted );
			modelInfo.Capture( source.asset, models[ selected ].name, pModel->vertices.size(), pModel->indices.size() / 3 );

			// Release CPU loading data on the worker, retaining the timings for the console
			const auto timings = source.timings;
			ClearSource();
			source.timings = timings;
			return true;
		}

		void DrainProgress()
		{
			std::lock_guard guard( progressMutex );
			for ( const auto &message : progress )
				if ( !exiting )
					log.Status( message );

			progress.clear();
		}

		void Step()
		{
			DrainProgress();

			if ( exiting )
				return;

			try
			{
				if ( stage == EStage::Idle && pending >= 0 )
				{
					if ( resources.pModel )
					{
						auto *pVulkan = resources.session.GetVulkan();
						auto previous = std::make_unique< SModelRetirement >( pVulkan->GetVkLogicalDevice(), *resources.textures );
						CheckVk( previous->Submit( pVulkan->GetVkQueue_Graphics() ), "Retire previous model" );

						std::erase( resources.info->vecRenderables, resources.pModel );
						previous->model.reset( std::exchange( resources.pModel, nullptr ) );
						retirement = std::move( previous );
						ready = false;
						playback = {};
						animationScroll = {};
						hover = {};
						modelInfo.lines.clear();
						stage = EStage::Retire;
						dirty = true;
						log.Status( "Releasing: " + models[ selected ].name );
						return;
					}

					Release();
					selected = std::exchange( pending, -1 );
					started = std::chrono::steady_clock::now();
					auto pModel = new CRenderModel( &resources.session, resources.info.get(), pipelines.pbrLayout, pipelines.pbr );
					pModel->isVisible = false;
					resources.pModel = pModel;

					SGltfLoadOptions options;
					options.textureDirectory = models[ selected ].textures;
					if ( const char *workers = std::getenv( "GLTFXR_DECODE_WORKERS" ) )
						options.imageDecodeWorkers = std::clamp( std::atoi( workers ), 1, 4 );

					options.onProgress = [ this ]( EGltfLoadStage next ) { Progress( next == EGltfLoadStage::ReadFile ? "Reading model file" : next == EGltfLoadStage::ParseAsset ? "Parsing and validating glTF" : "Reading and decoding textures" ); };
					loader = std::make_unique< CGltf >( &resources.session, options );
					log.Status( "Loading: " + models[ selected ].name );
					reading = std::async( std::launch::async, [ this, pModel ] { return Prepare( pModel ); } );
					stage = EStage::Read;
					dirty = true;
				}
				else if ( stage == EStage::Retire )
				{
					if ( retirement )
					{
						const auto result = retirement->Poll();
						if ( result == VK_NOT_READY )
							return;

						CheckVk( result, "Complete previous model rendering" );

						reading = std::async(
							std::launch::async,
							[ previous = std::move( retirement ) ]() mutable
							{
								previous.reset();
								return true;
							} );
					}
					else if ( reading.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready )
					{
						static_cast< void >( reading.get() );
						Release();
						stage = EStage::Idle;
					}
				}
				else if ( stage == EStage::Read && reading.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready )
				{
					const bool loaded = reading.get();
					DrainProgress();
					if ( !loaded )
						throw std::runtime_error( source.error.empty() ? "Couldn't load model" : source.error );

					log.Status( "Uploading textures" );
					uploadStarted = std::chrono::steady_clock::now();
					stage = EStage::Upload;
				}
				else if ( stage == EStage::Upload )
				{

					// The worker releases completed staging allocations before this pool is used again
					if ( reading.valid() )
					{
						if ( reading.wait_for( std::chrono::seconds( 0 ) ) != std::future_status::ready )
							return;

						static_cast< void >( reading.get() );
					}

					if ( uploadSubmitted )
					{
						const auto result = uploads[ uploadIndex ]->Poll();
						if ( result == VK_NOT_READY )
							return;

						CheckVk( result, "Complete texture upload" );
						reading = std::async(
							std::launch::async,
							[ completed = std::move( uploads[ uploadIndex ] ) ]() mutable
							{
								completed.reset();
								return true;
							} );
						++uploadIndex;
						uploadSubmitted = false;
						return;
					}

					if ( uploadIndex < uploads.size() )
					{
						log.Status( "Uploading texture " + std::to_string( uploadIndex + 1 ) + " / " + std::to_string( uploads.size() ) );
						CheckVk( uploads[ uploadIndex ]->Submit( resources.session.GetVulkan()->GetVkQueue_Graphics() ), "Submit texture upload" );
						uploadSubmitted = true;
					}
					else
					{
						source.timings.textureUploadMs = SLoadLog::Elapsed( uploadStarted );
						log.Status( "Binding materials" );
						stage = EStage::Bind;
					}
				}
				else if ( stage == EStage::Bind )
				{
					auto setup = std::chrono::steady_clock::now();
					uint32_t id;
					CheckVk( resources.info->pDescriptors->CreateDescriptorPool( id, pipelines.pbrFragmentDescriptorLayout, static_cast< uint32_t >( resources.pModel->materials.size() ) * resources.info->GetFramesInFlight() ) );
					pool = id;
					if ( resources.pModel->LoadMaterial( resources.info.get(), pipelines.pbrFragmentDescriptorLayout, id, resources.textures.get() ) != resources.pModel->materials.size() )
						throw std::runtime_error( "Material descriptor setup failed" );

					const double descriptorsMs = SLoadLog::Elapsed( setup );
					resources.info->vecRenderables.push_back( resources.pModel );
					ready = true;
					readyAt = std::chrono::steady_clock::now();
					stage = EStage::Idle;
					dirty = true;
					log.Status( "Ready: " + models[ selected ].name );
					log.Time( "Disk read", source.timings.diskReadMs );
					log.Time( "glTF parse + validation", source.timings.parseMs );
					log.Time( "Texture reads + decoding", source.timings.imageDecodeMs );
					log.Time( "Texture preparation + mesh conversion", prepareMs );
					log.Time( "Texture upload across frames", source.timings.textureUploadMs );
					log.Time( "Mesh conversion", source.timings.meshConversionMs );
					log.Time( "Placement", setupMs );
					log.Time( "Material descriptors", descriptorsMs );
					log.Time( "GPU mesh buffers", buffersMs );
					log.Time( "Elapsed including progress frames", SLoadLog::Elapsed( started ) );
					log.Add( "Vertices: " + std::to_string( resources.pModel->vertices.size() ) + " / triangles: " + std::to_string( resources.pModel->indices.size() / 3 ) );
					log.Add( HasAnimation() ? "Animation playing: " + AnimationName( playback.clip ) : "No animation" );
					ClearSource();
					loader.reset();
				}
			}
			catch ( XrResult result )
			{
				Fail( "XR error " + std::to_string( result ) );
			}
			catch ( const std::exception &error )
			{
				Fail( error.what() );
			}
		}

		void Fail( const std::string &message )
		{
			log.Status( "Loading failed: " + message );
			Release();
			selected = -1;
			stage = EStage::Idle;
			dirty = true;
		}

		bool CanExit()
		{
			if ( !exiting || ( reading.valid() && reading.wait_for( std::chrono::seconds( 0 ) ) != std::future_status::ready ) )
				return false;

			// Keep submitting frames until retired model draws and in-flight texture copies complete
			if ( retirement && retirement->Poll() == VK_NOT_READY )
				return false;

			if ( uploadSubmitted && uploads[ uploadIndex ]->Poll() == VK_NOT_READY )
				return false;

			return true;
		}

		void UpdateMesh( CRenderModel &outModel, SPanelHoverColors &outHover ) const
		{
			SPanelMesh mesh( outModel, outHover );
			mesh.Text( "GLTFXR XRLIB", -.425f, .225f );
			for ( bool animations : { false, true } )
			{
				const auto count = animations ? AnimationCount() : models.size();
				const auto &position = animations ? animationScroll : scroll;
				const bool enabled = !exiting && ( !animations || count > 0 );
				const float left = animations ? SPanelLayout::listGap : SPanelLayout::listLeft;
				const float right = animations ? SPanelLayout::listRight : -SPanelLayout::listGap;
				const float bottom = SPanelLayout::listTop - SPanelLayout::modelRows * SPanelLayout::rowHeight;
				const int scrollTarget = animations ? SPanelLayout::animationScrollArea : SPanelLayout::scrollArea;
				const int upTarget = animations ? SPanelLayout::animationUp : SPanelLayout::scrollUp;
				const int downTarget = animations ? SPanelLayout::animationDown : SPanelLayout::scrollDown;
				const XrVector3f textColor = enabled ? SPanelPalette::text : SPanelPalette::disabledText;
				mesh.Text( animations ? "Animations" : "Models", left, .18f, right - left, textColor );
				mesh.Quad( left, bottom, right, SPanelLayout::listTop, enabled ? SPanelPalette::listBackground : SPanelPalette::disabledBackground, 1, .001f );

				for ( int row = 0; row < SPanelLayout::modelRows && position.first + row < static_cast< int >( count ); ++row )
				{
					const int index = position.first + row;
					const int target = SPanelLayout::RowTarget( index, animations );
					const bool chosen = animations ? static_cast< size_t >( index ) == playback.clip : index == selected;
					const float top = SPanelLayout::listTop - row * SPanelLayout::rowHeight;
					mesh.Hoverable(
						target,
						[ & ]( bool hovered )
						{
							mesh.Quad( left, top - SPanelLayout::rowHeight + .004f, right, top, enabled && hovered ? SPanelPalette::hover : chosen ? SPanelPalette::selected : SPanelPalette::row, 1, .001f );
						} );

					// Keep selection recognisable beneath the system gaze highlight
					if ( chosen )
						mesh.Quad( left + .003f, top - SPanelLayout::rowHeight + .008f, left + .007f, top - .004f, enabled ? SPanelPalette::accent : SPanelPalette::disabledText, 1, .0015f );

					const std::string name = animations ? AnimationName( static_cast< size_t >( index ) ) : models[ index ].name + ( pending == index ? " (queued)" : chosen && stage != EStage::Idle ? " (loading)" : "" );
					mesh.Text( name, left + .015f, top - .01f, right - left - .03f, textColor );
				}

				if ( animations && count == 0 )
					mesh.Text( "No animations", left + .015f, SPanelLayout::listTop - .015f, right - left - .03f, textColor );

				if ( enabled && count > static_cast< size_t >( SPanelLayout::modelRows ) )
					mesh.Hoverable( scrollTarget, [ & ]( bool hovered ) { mesh.Outline( left, bottom, right, SPanelLayout::listTop, SPanelPalette::accent, hovered ); } );

				mesh.Hoverable( upTarget, [ & ]( bool hovered ) { mesh.ScrollArrow( true, right, enabled && position.first > 0, hovered, SPanelLayout::listButtonBottom, SPanelLayout::listButtonTop ); } );
				mesh.Hoverable(
					downTarget, [ & ]( bool hovered ) { mesh.ScrollArrow( false, right, enabled && position.first + SPanelLayout::modelRows < static_cast< int >( count ), hovered, SPanelLayout::listButtonBottom, SPanelLayout::listButtonTop ); } );
			}

			mesh.Text( exiting ? "Closing" : "Pinch + drag a list to scroll", -.425f, -.145f );
			mesh.Hoverable( SPanelLayout::mode, [ & ]( bool hovered ) { mesh.Button( passthrough ? "Immersive" : "Passthrough", -.205f, -.005f, !exiting && canChangeMode, hovered ); } );
			mesh.Hoverable( SPanelLayout::info, [ & ]( bool hovered ) { mesh.Button( "Info", .015f, .22f, !exiting && ready, hovered ); } );
			mesh.Hoverable( SPanelLayout::animation, [ & ]( bool hovered ) { mesh.PlaybackButton( playback.playing, !exiting && HasAnimation(), hovered ); } );
			mesh.Hoverable( SPanelLayout::exit, [ & ]( bool hovered ) { mesh.Button( "Exit", .24f, .425f, !exiting, hovered ); } );
		}
	};
} // namespace

int32_t RunGltfXr( SampleHostPoll pollHost, void *context )
{
	if ( !pollHost )
		return XR_ERROR_VALIDATION_FAILURE;

	try
	{
#ifdef XR_USE_PLATFORM_ANDROID
		CInstance instance( static_cast< android_app * >( context ), "gltfxr", 1 );
		Check( instance.InitAndroidLoader() );
#else
		CInstance instance( "gltfxr", 1 );
#endif
		std::vector< const char * > extensions { XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME, XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME };
#ifdef XR_USE_PLATFORM_ANDROID
		extensions.push_back( XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME );
		extensions.push_back( XR_FB_PASSTHROUGH_EXTENSION_NAME );
#endif
		std::vector< const char * > floorExtensions { XR_EXT_LOCAL_FLOOR_EXTENSION_NAME };
		Check( instance.RemoveUnsupportedExtensions( floorExtensions ) );
		extensions.insert( extensions.end(), floorExtensions.begin(), floorExtensions.end() );
		std::vector< const char * > layers;
		const void *pInstanceNext = nullptr;
#ifdef XR_USE_PLATFORM_ANDROID

		// XR_KHR_android_create_instance needs the app's VM and activity, the runtime rejects the instance without them
		XrInstanceCreateInfoAndroidKHR androidCreateInfo { XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR };
		androidCreateInfo.applicationVM = instance.GetAndroidApp()->activity->vm;
		androidCreateInfo.applicationActivity = instance.GetAndroidApp()->activity->clazz;
		pInstanceNext = &androidCreateInfo;
#endif
		Check( instance.Init( extensions, layers, 0, pInstanceNext ) );

		CSession session( &instance );

		// Content lives in LOCAL at head height, so floor estimates and corrections can't move it
		session.xrAppReferenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
		SSessionSettings settings;
#ifdef __APPLE__

		// MoltenVK requires this declaration for the existing multiview MSAA targets
		VkPhysicalDevicePortabilitySubsetFeaturesKHR portability { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR };
		portability.multisampleArrayImage = VK_TRUE;
		settings.pVkLogicalDeviceNext = &portability;
#endif
		Check( session.Init( settings ) );

		// Only the immersive grid uses the floor, located in LOCAL while it's visible
		XrSpace floorSpace = XR_NULL_HANDLE;
		XrReferenceSpaceCreateInfo floorInfo { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
		floorInfo.referenceSpaceType = floorExtensions.empty() ? XR_REFERENCE_SPACE_TYPE_STAGE : XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR;
		floorInfo.poseInReferenceSpace.orientation.w = 1;
		if ( XR_FAILED( xrCreateReferenceSpace( session.GetXrSession(), &floorInfo, &floorSpace ) ) )
			floorSpace = XR_NULL_HANDLE;

		panel_input::SPanelInput input;
		SPanelSystemHover systemHover;
		input.Init( session );
#ifdef XR_USE_PLATFORM_ANDROID
		FB::CPassthrough passthrough( instance.GetXrInstance() );
		Check( passthrough.Init( session.GetXrSession(), &instance ) );
		Check( passthrough.AddLayer( session.GetXrSession(), ExtBase_Passthrough::ELayerType::FULLSCREEN, XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT, XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB ) );
		Check( passthrough.Start() );
#endif

		uint32_t modeCount = 0;
		Check( xrEnumerateEnvironmentBlendModes( instance.GetXrInstance(), instance.GetXrSystemId(), XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &modeCount, nullptr ) );
		std::vector< XrEnvironmentBlendMode > modes( modeCount );
		Check( xrEnumerateEnvironmentBlendModes( instance.GetXrInstance(), instance.GetXrSystemId(), XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, modeCount, &modeCount, modes.data() ) );
#ifndef XR_USE_PLATFORM_ANDROID
		if ( std::find( modes.begin(), modes.end(), XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND ) == modes.end() )
			return XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED;
#endif

		const auto color = static_cast< VkFormat >( session.SelectColorTextureFormat( { VK_FORMAT_R8G8B8A8_SRGB } ) );
		const auto depth = static_cast< VkFormat >( session.SelectDepthTextureFormat( { VK_FORMAT_D32_SFLOAT } ) );
		if ( !color || !depth )
			return XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED;

		SRenderResources resources { session };
		resources.renderer = std::make_unique< CStereoRender >( &session, color, depth );
		auto &renderer = *resources.renderer;
		Check( renderer.Init() );
		VkRenderPass pass {};
		Check( renderer.CreateRenderPass( pass, false ) );
		resources.info = std::make_unique< CRenderInfo >( &session );
		auto &info = *resources.info;
		info.state.nearZ = .1f;
		info.state.farZ = 100;

		SPipelines pipelines;
		uint32_t pbrPipeline {};
		CheckVk( renderer.CreateGraphicsPipeline_PBR(
#ifdef XR_USE_PLATFORM_ANDROID
			instance.GetAndroidApp()->activity->assetManager,
#endif
			pipelines,
			pbrPipeline,
			&info,
			5,
			pass,
			"mesh_pbr_skin.vert.spv",
			"mesh_pbr.frag.spv",
			true,
			true ) );

		auto &lighting = *info.pSceneLighting;
		std::memset( &lighting, 0, sizeof( lighting ) );

		// Direction the light travels, a ceiling light 60 degrees up from behind the viewer's starting heading
		lighting.mainLight.direction = { 0, -.8660254f, -.5f };
		lighting.mainLight.color = { 1, 1, 1 };
		lighting.mainLight.intensity = 1.f;
		lighting.ambientColor = { 1, 1, 1 };
		lighting.ambientIntensity = 0.0f;
		lighting.tonemapping = { 1, 1, 0, 1, 1 }; // Linear output, the SRGB attachment encodes once
		lighting.tonemapping.setRenderMode( ERenderMode::PBR );
		lighting.tonemapping.setTonemapOperator( ETonemapOperator::KHRNeutral );

		// Studio lighting suits passthrough, the night grid lights immersive mode to match its backdrop
		const auto LoadEnvironment = [ & ]( const char *filename )
		{
			const auto bytes = ReadBinaryFile(
#ifdef XR_USE_PLATFORM_ANDROID
				instance.GetAndroidApp()->activity->assetManager,
#endif
				filename );
			const auto data = DecodeEnvironment( { reinterpret_cast< const uint8_t * >( bytes.data() ), bytes.size() } );
			auto environment = std::make_shared< CEnvironmentLighting >();
			CheckVk( environment->Init( renderer.GetLogicalDevice(), renderer.GetPhysicalDevice(), renderer.GetCommandPool(), session.GetVulkan()->GetVkQueue_Graphics(), data ), "IBL upload" );
			return environment;
		};

		constexpr float studioIntensity = 0.25f, nightIntensity = 1.f;
		resources.studio = LoadEnvironment( "PreparedStudio/studio.ibl" );
		resources.night = LoadEnvironment( "PreparedNightGrid/night_grid.ibl" );
		CheckVk( info.SetEnvironment( resources.studio, studioIntensity, 0.f ), "Studio IBL descriptors" );
		LogInfo( "gltfxr", "Studio and night grid IBL ready" );

		resources.textures = std::make_unique< CTextureManager >( &session, renderer.GetCommandPool() );

		// Prepare the shared fallback before interactive frame rendering starts
		resources.textures->GetDefaultTexture();

		SLoadLog log;
		log.Add( log.status );
		auto panelState = renderer.CreateDefaultPipelineState( renderer.GetTextureWidth(), renderer.GetTextureHeight() );
		panelState.rasterization.cullMode = VK_CULL_MODE_NONE;
		panelState.depthStencil.depthTestEnable = VK_FALSE;
		panelState.depthStencil.depthWriteEnable = VK_FALSE;
		panelState.colorBlendAttachments[ 0 ] = renderer.GenerateColorBlendAttachment( true );
		panelState.colorBlendAttachments[ 0 ].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

		uint32_t panelPipeline = 0;
		CheckVk( renderer.CreateGraphicsPipeline_CustomPBR(
#ifdef XR_USE_PLATFORM_ANDROID
			instance.GetAndroidApp()->activity->assetManager,
#endif
			pipelines,
			panelPipeline,
			&info,
			pass,
			"log.vert.spv",
			"log.frag.spv",
			panelState,
			false,
			5 ) );
		auto pPanel = new CRenderModel( &session, &info, pipelines.pbrLayout, panelPipeline );
		auto pControls = new CRenderModel( &session, &info, pipelines.pbrLayout, panelPipeline );
		info.vecRenderables.push_back( pPanel );
		info.vecRenderables.push_back( pControls );

		// Cull the back of the tube so transparency doesn't accumulate through both walls
		panelState.rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
		uint32_t rayPipeline = 0;
		CheckVk( renderer.CreateGraphicsPipeline_CustomPBR(
#ifdef XR_USE_PLATFORM_ANDROID
			instance.GetAndroidApp()->activity->assetManager,
#endif
			pipelines,
			rayPipeline,
			&info,
			pass,
			"log.vert.spv",
			"log.frag.spv",
			panelState,
			false,
			5 ) );
		CRenderModel *pRays[ 2 ] {};
		for ( auto &pRay : pRays )
		{
			pRay = new CRenderModel( &session, &info, pipelines.pbrLayout, rayPipeline );
			info.vecRenderables.push_back( pRay );
			SAimRay::InitMesh( *pRay );
			CheckVk( pRay->InitBuffers() );
			pRay->isVisible = false;
		}

		// Loading ring and label, the ring keeps a fixed vertex count so it can update in place
		auto pRing = new CRenderModel( &session, &info, pipelines.pbrLayout, panelPipeline );
		auto pLabel = new CRenderModel( &session, &info, pipelines.pbrLayout, panelPipeline );
		SLoadIndicator::InitRing( *pRing );
		CheckVk( pRing->InitBuffers() );
		for ( auto pIndicator : { pRing, pLabel } )
		{
			pIndicator->isVisible = false;
			info.vecRenderables.push_back( pIndicator );
		}

		SLoadIndicator::SProgress indicatorLabel;

		// Frame timing graph and numbers over the console panel, fixed size so they update in place
		auto pGraph = new CRenderModel( &session, &info, pipelines.pbrLayout, panelPipeline );
		SPerformancePanel::InitDynamic( *pGraph );
		CheckVk( pGraph->InitBuffers() );
		pGraph->isVisible = false;
		info.vecRenderables.push_back( pGraph );

		// Night grid for immersive mode, drawn before the model. The opaque floor writes depth first, then the
		// sky sits at the far plane so the depth test skips every pixel the floor already covers
		auto backdropState = renderer.CreateDefaultPipelineState( renderer.GetTextureWidth(), renderer.GetTextureHeight() );
		backdropState.rasterization.cullMode = VK_CULL_MODE_NONE;

		uint32_t skyPipeline = 0, floorPipeline = 0;
		CheckVk( renderer.CreateGraphicsPipeline_CustomPBR(
#ifdef XR_USE_PLATFORM_ANDROID
			instance.GetAndroidApp()->activity->assetManager,
#endif
			pipelines,
			floorPipeline,
			&info,
			pass,
			"backdrop.vert.spv",
			"night_grid_floor.frag.spv",
			backdropState,
			false,
			5 ) );

		backdropState.depthStencil.depthWriteEnable = VK_FALSE;
		backdropState.depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		CheckVk( renderer.CreateGraphicsPipeline_CustomPBR(
#ifdef XR_USE_PLATFORM_ANDROID
			instance.GetAndroidApp()->activity->assetManager,
#endif
			pipelines,
			skyPipeline,
			&info,
			pass,
			"backdrop_far.vert.spv",
			"night_grid_sky.frag.spv",
			backdropState,
			false,
			5 ) );

		// Stars and shooting stars add their glow over the sky, still behind anything that wrote depth
		auto &starBlend = backdropState.colorBlendAttachments.front();
		starBlend.blendEnable = VK_TRUE;
		starBlend.srcColorBlendFactor = starBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		starBlend.colorBlendOp = VK_BLEND_OP_ADD;
		starBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		starBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		starBlend.alphaBlendOp = VK_BLEND_OP_ADD;

		uint32_t starPipeline = 0;
		CheckVk( renderer.CreateGraphicsPipeline_CustomPBR(
#ifdef XR_USE_PLATFORM_ANDROID
			instance.GetAndroidApp()->activity->assetManager,
#endif
			pipelines,
			starPipeline,
			&info,
			pass,
			"night_grid_stars.vert.spv",
			"night_grid_stars.frag.spv",
			backdropState,
			false,
			5 ) );

		const auto skyBytes = ReadBinaryFile(
#ifdef XR_USE_PLATFORM_ANDROID
			instance.GetAndroidApp()->activity->assetManager,
#endif
			"PreparedNightGrid/night_grid.sky" );
		auto sky = DecodeBackground( { reinterpret_cast< const uint8_t * >( skyBytes.data() ), skyBytes.size() } );

		STexture skyTexture;
		CheckVk( resources.textures->CreateCubeTextureFromData( skyTexture, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, sky.pixels.data(), sky.size ), "Night sky upload" );
		resources.images.push_back( skyTexture );

		uint32_t backdropPool = 0;
		CheckVk( info.pDescriptors->CreateDescriptorPool( backdropPool, pipelines.pbrFragmentDescriptorLayout, 4 * info.GetFramesInFlight() ) );

		auto pSky = new CRenderModel( &session, &info, pipelines.pbrLayout, skyPipeline );
		auto pFloor = new CRenderModel( &session, &info, pipelines.pbrLayout, floorPipeline );
		auto pStars = new CRenderModel( &session, &info, pipelines.pbrLayout, starPipeline );
		auto pShootingStar = new CRenderModel( &session, &info, pipelines.pbrLayout, starPipeline );
		std::vector< SMaterialUBO * > skyMaterial;
		pSky->textures = { skyTexture };
		SBackdrop::InitSky( *pSky, 0 );
		SBackdrop::InitFloor( *pFloor );
		SBackdrop::InitStars( *pStars );

		SShootingStar shootingStar;
		shootingStar.Init( *pShootingStar, 0 );

		if ( pSky->LoadMaterial( skyMaterial, &info, pipelines.pbrFragmentDescriptorLayout, backdropPool, resources.textures.get() ) != 1 ||
			 pFloor->LoadMaterial( &info, pipelines.pbrFragmentDescriptorLayout, backdropPool, resources.textures.get() ) != 1 || pStars->LoadMaterial( &info, pipelines.pbrFragmentDescriptorLayout, backdropPool, resources.textures.get() ) != 1 ||
			 pShootingStar->LoadMaterial( &info, pipelines.pbrFragmentDescriptorLayout, backdropPool, resources.textures.get() ) != 1 )
			throw std::runtime_error( "Backdrop descriptor setup failed" );

		// Background keeps list order, so the floor's depth is in place before the sky, then stars blend over it
		for ( auto pBackdrop : { pFloor, pSky, pStars, pShootingStar } )
		{
			pBackdrop->renderQueue = ERenderQueue::Background;
			pBackdrop->isVisible = false;
			CheckVk( pBackdrop->InitBuffers() );
			info.vecRenderables.push_back( pBackdrop );
		}

		// Clock for the loading ring spin and shooting stars
		const auto animationStarted = std::chrono::steady_clock::now();

		SFrameStats frameStats;
		auto frameMark = animationStarted, statsShown = animationStarted;
		XrTime previousDisplayTime = 0;
		SPerformancePanel::SReadout readout;
		double shownPeriod = -1;
		uint64_t shownColumn = UINT64_MAX;
		std::vector< uint32_t > controlsIndices, panelIndices;
		SPanelHoverColors controlsHover, panelHover;
		bool nightLighting = false;

		bool suppressRays = false;
#if defined( __APPLE__ ) && TARGET_OS_VISION
		suppressRays = true;
#endif

		SModelViewer viewer { resources, pipelines, log };
#ifdef XR_USE_PLATFORM_ANDROID
		viewer.canChangeMode = true;
#else

		// Immersive mode needs the host to allow both immersion styles
		viewer.canChangeMode = std::find( modes.begin(), modes.end(), XR_ENVIRONMENT_BLEND_MODE_OPAQUE ) != modes.end();
#endif

		// Start with the helmet, loading begins once the scene is placed
		viewer.Select( 0 );
		SSceneState scene;
		std::vector< CPlane2D * > masks;
		bool running = false, exitRequested = false;


		while ( pollHost( context ) && session.GetState() != XR_SESSION_STATE_EXITING )
		{

			// Drain lifecycle events before deciding whether another frame may begin
			for ( ;; )
			{
				XrEventDataBaseHeader event { XR_TYPE_EVENT_DATA_EVENTS_LOST };
				const auto result = session.Poll( &event );
				if ( result == XR_EVENT_UNAVAILABLE )
					break;

				Check( result );
				if ( event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED )
				{
					if ( session.GetState() == XR_SESSION_STATE_READY && !running )
					{
						Check( session.Start() );
						running = true;
					}

					if ( session.GetState() == XR_SESSION_STATE_STOPPING && running )
					{
						Check( session.End() );
						running = false;
					}
				}
			}

			if ( session.GetState() == XR_SESSION_STATE_EXITING || session.GetState() == XR_SESSION_STATE_LOSS_PENDING )
				break;

			if ( !running )
			{
				std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );
				continue;
			}

			// Poll loading and submit one prepared texture at a time between frames
			if ( scene.placed && info.state.frameState.shouldRender )
				viewer.Step();

			// Panels rebuild when their content changes, hover only swaps the recorded vertices of the affected elements
			if ( viewer.dirty || viewer.hover != controlsHover.applied )
			{
				if ( viewer.dirty )
					viewer.UpdateMesh( *pControls, controlsHover );

				controlsHover.Apply( pControls->vertices, viewer.hover );
				UploadPanel( *pControls, controlsIndices );
				viewer.dirty = false;
			}

			// The performance numbers refresh a few times a second, its labels only when the view or period changes
			const auto panelTime = std::chrono::steady_clock::now();
			const bool readoutUpdated = log.performance && ( log.dirty || panelTime - statsShown >= std::chrono::milliseconds( 250 ) );
			if ( readoutUpdated )
			{
				readout = SPerformancePanel::Readout( frameStats );
				statsShown = panelTime;
			}

			const bool panelChanged = log.dirty || ( log.performance && std::abs( readout.targetMs - shownPeriod ) > .005 );
			if ( panelChanged || log.hover != panelHover.applied )
			{
				if ( panelChanged && log.performance )
				{
					SPerformancePanel::UpdateMesh( *pPanel, readout.targetMs, panelHover );
					shownPeriod = readout.targetMs;
				}
				else if ( panelChanged )
					log.UpdateMesh( *pPanel, panelHover );

				panelHover.Apply( pPanel->vertices, log.hover );
				UploadPanel( *pPanel, panelIndices );
				log.dirty = false;
			}

			// Each frame in flight rebinds the lighting descriptors once its earlier work has finished
			if ( nightLighting == viewer.passthrough )
			{
				nightLighting = !viewer.passthrough;
				CheckVk( info.SetEnvironment( nightLighting ? resources.night : resources.studio, nightLighting ? nightIntensity : studioIntensity, 0.f ), "Switch IBL" );
			}

			const auto waitStarted = std::chrono::steady_clock::now();
			Check( session.StartFrame( &info.state.frameState ) );
			const double waitMs = SLoadLog::Elapsed( waitStarted );
			XrSpaceLocation head { XR_TYPE_SPACE_LOCATION };
			Check( xrLocateSpace( session.GetHmdSpace(), session.GetAppSpace(), info.state.frameState.predictedDisplayTime, &head ) );
			constexpr XrSpaceLocationFlags headTracked = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
			if ( !scene.placed && !viewer.exiting && info.state.frameState.shouldRender && ( head.locationFlags & headTracked ) == headTracked && scene.Place( head.pose ) )
			{
				fprintf(
					stderr,
					"GLTFXR_PLACED: headY=%.3f fallbackFloorY=%.3f centre=(%.3f, %.3f, %.3f) distance=%.3f headFlags=0x%llx\n",
					head.pose.position.y,
					scene.floorY,
					scene.position.x,
					scene.position.y,
					scene.position.z,
					SSceneState::distance,
					static_cast< unsigned long long >( head.locationFlags ) );

				if ( viewer.pending < 0 )
					log.Status( "Choose a model from the left panel" );
			}

			const auto regions = viewer.Regions();
			if ( !exitRequested )
				input.Update(
					scene,
					session,
					info.state.frameState.predictedDisplayTime,
					regions,
					[ & ]( int side, int target ) { viewer.Click( side, target ); },
					[ & ]( int side, int list, float delta ) { viewer.Drag( side, list, delta ); },
					[ & ]( int hand, int side, int target ) { viewer.Hover( hand, side, target ); } );

			Check( systemHover.Submit( scene, session, info.state.frameState.predictedDisplayTime, regions ) );

			if ( viewer.ready )
			{
				auto pModel = resources.pModel;
				if ( viewer.playback.playing && info.state.frameState.shouldRender )
				{
					const auto time = info.state.frameState.predictedDisplayTime;
					if ( !viewer.playback.epoch )
						viewer.playback.epoch = time;

					viewer.playback.Update( *pModel, static_cast< double >( time - viewer.playback.epoch ) * 1e-9 );

					// EndRenderFrame waits for the previous draw before the pose buffer is reused
					CheckVk( pModel->UpdateSkinning(), "Update skinning matrices" );
				}

				// Grow in from slightly smaller so the swap from the loading ring doesn't pop
				constexpr float appearSeconds = .3f;
				const float appear = std::clamp( std::chrono::duration< float >( std::chrono::steady_clock::now() - viewer.readyAt ).count() / appearSeconds, 0.f, 1.f );
				const float scale = .9f + .1f * appear * appear * ( 3.f - 2.f * appear );
				pModel->instances[ 0 ].pose = { scene.orientation, scene.position };
				pModel->instances[ 0 ].scale = { scale, scale, scale };
				pModel->isVisible = scene.placed;
			}

			// Loading indicator in the empty model area
			const auto progress = viewer.LoadProgress();
			pRing->isVisible = pLabel->isVisible = progress && scene.placed && !exitRequested && info.state.frameState.shouldRender;
			if ( pRing->isVisible )
			{
				if ( progress->name != indicatorLabel.name || progress->stage != indicatorLabel.stage )
				{
					indicatorLabel = *progress;
					SLoadIndicator::UpdateLabel( *pLabel, indicatorLabel );
					CheckVk( pLabel->InitBuffers() );
				}

				SLoadIndicator::UpdateRing( pRing->vertices, progress->fraction, std::chrono::duration< float >( std::chrono::steady_clock::now() - animationStarted ).count() );
				CheckVk( pRing->UpdateVertexBuffer(), "Update loading ring" );
				pRing->instances[ 0 ].pose = pLabel->instances[ 0 ].pose = SLoadIndicator::Pose( scene.position, head.pose.position );
			}

			for ( int hand = 0; hand < 2; ++hand )
			{
				auto pRay = pRays[ hand ];
				const auto &sample = input.rays[ hand ];
				pRay->isVisible = scene.placed && !viewer.exiting && !exitRequested && info.state.frameState.shouldRender && SAimRay::Visible( sample, head, suppressRays );
				if ( pRay->isVisible )
				{
					pRay->instances[ 0 ].pose = sample.pose;
					SAimRay::UpdateVertices( pRay->vertices, SAimRay::Length( scene, sample.pose ) );
					CheckVk( pRay->UpdateVertexBuffer(), "Update aim ray vertex buffer" );
				}

				// Draw transparent rays after the current model and panel geometry
				std::erase( info.vecRenderables, pRay );
				info.vecRenderables.push_back( pRay );
			}

			pPanel->instances[ 0 ].pose = SPanelLayout::Pose( scene, 1 );
			pControls->instances[ 0 ].pose = SPanelLayout::Pose( scene, -1 );
			pPanel->isVisible = pControls->isVisible = scene.placed;
			pGraph->isVisible = log.performance && scene.placed && info.state.frameState.shouldRender;
			pGraph->instances[ 0 ].pose = pPanel->instances[ 0 ].pose;

			// Only when a graph column completes or the numbers refresh
			const uint64_t graphColumn = frameStats.frames / frameStats.FramesPerColumn();
			if ( pGraph->isVisible && ( graphColumn != shownColumn || readoutUpdated ) )
			{
				SPerformancePanel::UpdateDynamic( pGraph->vertices, frameStats, readout );
				CheckVk( pGraph->UpdateVertexBuffer(), "Update performance graph" );
				shownColumn = graphColumn;
			}

			// Keep the sky centred on the head so it reads as distant, the floor stays under the model
			pSky->isVisible = pStars->isVisible = !viewer.passthrough;
			pFloor->isVisible = !viewer.passthrough && scene.placed;
			if ( head.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT )
				pSky->instances[ 0 ].pose.position = pStars->instances[ 0 ].pose.position = pShootingStar->instances[ 0 ].pose.position = head.pose.position;

			const float seconds = std::chrono::duration< float >( std::chrono::steady_clock::now() - animationStarted ).count();
			pShootingStar->isVisible = shootingStar.Update( pShootingStar->vertices, seconds, head.pose.orientation ) && !viewer.passthrough;
			if ( pShootingStar->isVisible )
				CheckVk( pShootingStar->UpdateVertexBuffer(), "Update shooting star" );

			if ( pFloor->isVisible && floorSpace != XR_NULL_HANDLE )
			{
				XrSpaceLocation floor { XR_TYPE_SPACE_LOCATION };
				if ( XR_SUCCEEDED( xrLocateSpace( floorSpace, session.GetAppSpace(), info.state.frameState.predictedDisplayTime, &floor ) ) && scene.UpdateFloor( floor ) )
					fprintf( stderr, "GLTFXR_FLOOR: floorY=%.3f headY=%.3f\n", scene.floorY, scene.position.y );
			}

			pFloor->instances[ 0 ].pose = { scene.orientation, { scene.position.x, scene.floorY, scene.position.z } };
#ifdef XR_USE_PLATFORM_ANDROID

			// Pause the camera feed in immersive mode rather than only hiding its layer
			Check( viewer.passthrough ? passthrough.Start() : passthrough.Stop() );
			if ( viewer.passthrough )
				passthrough.GetCompositionLayers( info.state.preAppFrameLayers );
			else
				info.state.preAppFrameLayers.clear();

			info.state.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
#else
			info.state.environmentBlendMode = viewer.passthrough ? XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND : XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
#endif
			info.state.compositionLayerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
			info.state.clearValues[ 0 ].color.float32[ 3 ] = info.state.clearValues[ 1 ].color.float32[ 3 ] = viewer.passthrough ? 0.f : 1.f;
			if ( exitRequested || viewer.CanExit() )
			{

				// Remove the app projection before teardown, retaining passthrough where supported
				std::vector< XrCompositionLayerBaseHeader * > layers;
				if ( info.state.frameState.shouldRender )
					layers = info.state.preAppFrameLayers;

				Check( session.EndFrame( &info.state.frameState, layers, info.state.environmentBlendMode ) );

				if ( !exitRequested )
				{
					Check( xrRequestExitSession( session.GetXrSession() ) );
					exitRequested = true;
				}
			}
			else
				Check( renderer.EndRenderFrame( pass, &info, masks ) );

			// Loop time outside xrWaitFrame counts as CPU, including runtime swapchain waits and xrvk waiting for the GPU to finish the frame
			const auto frameEnd = std::chrono::steady_clock::now();
			const double loopMs = std::chrono::duration< double, std::milli >( frameEnd - frameMark ).count();
			const XrTime displayTime = info.state.frameState.predictedDisplayTime;
			frameStats.Add( info.state.frameState.predictedDisplayPeriod * 1e-6, previousDisplayTime ? ( displayTime - previousDisplayTime ) * 1e-6 : 0., std::max( 0., loopMs - waitMs ), waitMs );
			frameMark = frameEnd;
			previousDisplayTime = displayTime;
		}

		if ( floorSpace != XR_NULL_HANDLE )
			xrDestroySpace( floorSpace );

		return XR_SUCCESS;
	}
	catch ( XrResult result )
	{
		fprintf( stderr, "GLTFXR_ERROR: %d\n", result );
		return result;
	}
	catch ( const std::exception &error )
	{
		fprintf( stderr, "GLTFXR_ERROR: %s\n", error.what() );
		return XR_ERROR_RUNTIME_FAILURE;
	}
}

#ifdef XR_USE_PLATFORM_ANDROID
void android_main( android_app *pApp )
{
	pApp->onAppCmd = app_handle_cmd;
	RunGltfXr(
		[]( void *context )
		{
			auto pAndroidApp = static_cast< android_app * >( context );
			int events;
			android_poll_source *source;
			while ( ALooper_pollOnce( 0, nullptr, &events, reinterpret_cast< void ** >( &source ) ) >= 0 )
				if ( source )
					source->process( pAndroidApp, source );

			return !pAndroidApp->destroyRequested;
		},
		pApp );

	// Keep handling glue commands until Android destroys the activity, otherwise its pause waits on this thread forever
	ANativeActivity_finish( pApp->activity );
	while ( !pApp->destroyRequested )
	{
		int events;
		android_poll_source *source;
		if ( ALooper_pollOnce( -1, nullptr, &events, reinterpret_cast< void ** >( &source ) ) >= 0 && source )
			source->process( pApp, source );
	}
}
#endif
