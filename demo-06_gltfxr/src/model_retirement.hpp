// Copyright 2026 Rune Berg. SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <xrvk/mesh.hpp>
#include <xrvk/texture.hpp>

// Detach on the frame thread, then destroy on a worker after the queue marker completes
struct SModelRetirement
{
	VkDevice device;
	xrlib::CTextureManager &textures;
	std::unique_ptr< xrlib::CRenderModel > model;
	VkFence fence = VK_NULL_HANDLE;
	bool submitted = false, complete = false;

	SModelRetirement( VkDevice device, xrlib::CTextureManager &textures )
		: device( device )
		, textures( textures )
	{
	}

	SModelRetirement( const SModelRetirement & ) = delete;
	SModelRetirement &operator=( const SModelRetirement & ) = delete;

	~SModelRetirement()
	{

		// Shutdown and error paths can arrive before the normal nonblocking poll completes
		if ( submitted && !complete )
			vkWaitForFences( device, 1, &fence, VK_TRUE, UINT64_MAX );

		if ( model )
			for ( auto &texture : model->textures )
				textures.DestroyTexture( texture );
		model.reset();
		if ( fence )
			vkDestroyFence( device, fence, nullptr );
	}

	VkResult Submit( VkQueue queue )
	{
		if ( fence )
			return VK_ERROR_INITIALIZATION_FAILED;

		VkFenceCreateInfo info { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		auto result = vkCreateFence( device, &info, nullptr, &fence );
		if ( result != VK_SUCCESS )
			return result;

		VkSubmitInfo marker { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		result = vkQueueSubmit( queue, 1, &marker, fence );
		submitted = result == VK_SUCCESS;
		return result;
	}

	VkResult Poll()
	{
		if ( !submitted )
			return VK_ERROR_INITIALIZATION_FAILED;

		const auto result = vkGetFenceStatus( device, fence );
		complete = result == VK_SUCCESS || result == VK_ERROR_DEVICE_LOST;
		return result;
	}
};
