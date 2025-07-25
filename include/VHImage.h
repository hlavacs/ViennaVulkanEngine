#pragma once


namespace vvh {

		//---------------------------------------------------------------------------------------------

		struct ImgTransitionImageLayoutInfo {
			const VkDevice& m_device; 
			const VkQueue& m_graphicsQueue; 
			const VkCommandPool& m_commandPool;
			const VkImage& m_image; 
			const VkFormat& m_format;
			const VkImageAspectFlags& m_aspect; 
			const int& m_mipLevels; 
			const int& m_layers;
			const VkImageLayout& m_oldLayout; 
			const VkImageLayout& m_newLayout;
		};
	
		template<typename T = ImgTransitionImageLayoutInfo>
		inline void ImgTransitionImageLayout(T&& info) {
		   VkCommandBuffer commandBuffer = ComBeginSingleTimeCommands(info);
	
		   VkImageMemoryBarrier barrier{};
		   barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		   barrier.oldLayout = info.m_oldLayout;
		   barrier.newLayout = info.m_newLayout;
		   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		   barrier.image = info.m_image;
		   barrier.subresourceRange.aspectMask = info.m_aspect;
		   barrier.subresourceRange.baseMipLevel = 0;
		   barrier.subresourceRange.levelCount = info.m_mipLevels;
		   barrier.subresourceRange.baseArrayLayer = 0;
		   barrier.subresourceRange.layerCount = info.m_layers;
	
		   VkPipelineStageFlags sourceStage;
		   VkPipelineStageFlags destinationStage;
	
		   if (info.m_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && info.m_newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			   barrier.srcAccessMask = 0;
			   barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			   sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			   destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		   } else if (info.m_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && info.m_newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			   barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			   barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			   sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			   destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		   } else if(info.m_oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && info.m_newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
			   barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			   barrier.dstAccessMask = 0;
			   sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			   destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		   } else if(info.m_oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && info.m_newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
			   barrier.srcAccessMask = 0;
			   barrier.dstAccessMask = 0;
			   sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			   destinationStage = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		   } else if(info.m_oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && info.m_newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			   barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			   barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
			   sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			   destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		   } else {
			   barrier.srcAccessMask = 0;
			   barrier.dstAccessMask = 0;
			   sourceStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			   destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		   }
	
		   vkCmdPipelineBarrier(
				 commandBuffer,
				 sourceStage, 
				 destinationStage,
				 0,
				 0, nullptr,
				 0, nullptr,
				 1, &barrier
		   );
	
		   ComEndSingleTimeCommands({info.m_device, info.m_graphicsQueue, info.m_commandPool, commandBuffer});
	   }
	
		//---------------------------------------------------------------------------------------------
		
		struct ImgTransitionImageLayout2Info {
			const VkDevice& m_device; 
			const VkQueue& m_graphicsQueue; 
			const VkCommandPool& m_commandPool;
			const VkImage& m_image; 
			const VkFormat& m_format; 
			const VkImageLayout& m_oldLayout; 
			const VkImageLayout& m_newLayout;
		};
	
		template<typename T = ImgTransitionImageLayout2Info>
		inline void ImgTransitionImageLayout2(T&& info) {
			ImgTransitionImageLayout( {
				info.m_device, 
				info.m_graphicsQueue, 
				info.m_commandPool, 
				info.m_image, 
				info.m_format, 
				VK_IMAGE_ASPECT_COLOR_BIT, 
				1, 
				1, 
				info.m_oldLayout, 
				info.m_newLayout
			});
		}
    
	//---------------------------------------------------------------------------------------------

	struct ImgCreateTextureSamplerInfo {
		const VkPhysicalDevice& m_physicalDevice;
		const VkDevice& 		m_device;
		Image& 					m_texture;
	};

	template<typename T = ImgCreateTextureSamplerInfo>
	inline void ImgCreateTextureSampler(T&& info) {
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(info.m_physicalDevice, &properties);
  
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  
		if (vkCreateSampler(info.m_device, &samplerInfo, nullptr, &info.m_texture.m_mapSampler) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture sampler!");
		}
	}
  

 	//---------------------------------------------------------------------------------------------

	struct ImgCreateImageViewInfo {
		const VkDevice& 			m_device;
		const VkImage& 				m_image;
		const VkFormat& 			m_format;
		const VkImageAspectFlags& 	m_aspects;
		const uint32_t& 			m_layers;
		const uint32_t& 			m_mipLevels;
	}; 
	
	template<typename T = ImgCreateImageViewInfo>
	auto ImgCreateImageView(T&& info) -> VkImageView {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = info.m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = info.m_format;
        viewInfo.subresourceRange.aspectMask = info.m_aspects;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = info.m_mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = info.m_layers;

        VkImageView imageView;
        if (vkCreateImageView(info.m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }

        return imageView;
    }


	//---------------------------------------------------------------------------------------------
	
	struct ImgCreateImageView2Info {
		const VkDevice& 	m_device; 
		const VkImage& 		m_image;
		const VkFormat& 	m_format;
		const VkImageAspectFlags& m_aspects;
	};

	template<typename T = ImgCreateImageView2Info>
	inline auto ImgCreateImageView2(T&& info) -> VkImageView {
		return ImgCreateImageView({
			.m_device 		= info.m_device, 
			.m_image 		= info.m_image, 
			.m_format 		= info.m_format, 
			.m_aspects 		= info.m_aspects, 
			.m_layers 		= 1, 
			.m_mipLevels 	= 1 
		});
	}
	
	//---------------------------------------------------------------------------------------------

	struct ImgCreateTextureImageViewinfo {
		const VkDevice& m_device;
		Image& m_texture;
	};
	
	template<typename T = ImgCreateTextureImageViewinfo>
	inline void ImgCreateTextureImageView(T&& info) {
		info.m_texture.m_mapImageView = ImgCreateImageView2({
			.m_device 	= info.m_device, 
			.m_image 	= info.m_texture.m_mapImage, 
			.m_format 	= VK_FORMAT_R8G8B8A8_SRGB,
			.m_aspects 	= VK_IMAGE_ASPECT_COLOR_BIT
		});
	}
	  
	//---------------------------------------------------------------------------------------------

	struct ImgCreateImageInfo {
		const VkPhysicalDevice& 	m_physicalDevice; 
		const VkDevice& 			m_device; 
		const VmaAllocator& 		m_vmaAllocator;
		const uint32_t& 			m_width; 
		const uint32_t& 			m_height; 
		const uint32_t& 			m_depth; 
		const uint32_t& 			m_layers; 
		const uint32_t& 			m_mipLevels;
		const VkFormat& 			m_format; 
		const VkImageTiling& 		m_tiling; 
		const VkImageUsageFlags& 	m_usage; 
		const VkImageLayout& 		m_imageLayout;
		const VkMemoryPropertyFlags& m_properties; 
		VkImage& 		m_image; 
		VmaAllocation& 	m_imageAllocation;
	};

	template<typename T = ImgCreateImageInfo>
	inline void ImgCreateImage(T&& info) {
		VkImageCreateInfo imageInfo{};
		imageInfo.sType 		= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType		= VK_IMAGE_TYPE_2D;
		imageInfo.extent.width 	= info.m_width;
		imageInfo.extent.height = info.m_height;
		imageInfo.extent.depth 	= info.m_depth;
		imageInfo.mipLevels 	= info.m_mipLevels;
		imageInfo.arrayLayers 	= info.m_layers;
		imageInfo.format 		= info.m_format;
		imageInfo.tiling 		= info.m_tiling;
		imageInfo.initialLayout = info.m_imageLayout;
		imageInfo.usage 		= info.m_usage;
		imageInfo.samples 		= VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode 	= VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
		allocInfo.priority = 1.0f;
		vmaCreateImage(info.m_vmaAllocator, &imageInfo, &allocInfo, &info.m_image, &info.m_imageAllocation, nullptr);
	}
		
	//---------------------------------------------------------------------------------------------

    struct ImgCreateImage2Info{ 
		const VkPhysicalDevice& m_physicalDevice; 
		const VkDevice& 		m_device; 
		const VmaAllocator& 	m_vmaAllocator; 
		const uint32_t& 		m_width; 
		const uint32_t& 		m_height; 
		const VkFormat& 		m_format; 
		const VkImageTiling& 	m_tiling;
		const VkImageUsageFlags& m_usage; 
		const VkImageLayout& 	m_imageLayout; 
		const VkMemoryPropertyFlags& m_properties; 
		VkImage& 		m_image; 
		VmaAllocation& 	m_imageAllocation;
	};

	template<typename T = ImgCreateImage2Info>
	inline void ImgCreateImage2(T&& info) {
			return ImgCreateImage({
					.m_physicalDevice 	= info.m_physicalDevice, 
					.m_device 			= info.m_device, 
					.m_vmaAllocator 	= info.m_vmaAllocator, 
					.m_width 			= info.m_width, 
					.m_height 			= info.m_height, 
					.m_depth 			= 1, 
					.m_layers 			= 1, 
					.m_mipLevels 		= 1, 
					.m_format 			= info.m_format, 
					.m_tiling 			= info.m_tiling, 
					.m_usage 			= info.m_usage, 
					.m_imageLayout 		= info.m_imageLayout, 
					.m_properties 		= info.m_properties, 
					.m_image 			= info.m_image, 
					.m_imageAllocation 	= info.m_imageAllocation
				});
	}

	//---------------------------------------------------------------------------------------------
	
	struct ImgCreateTextureImageInfo {
		const VkPhysicalDevice& m_physicalDevice;
		const VkDevice& 		m_device;
		const VmaAllocator& 	m_vmaAllocator;
		const VkQueue& 			m_graphicsQueue;
		const VkCommandPool& 	m_commandPool;
		const void* 			m_pixels;
		const int& 				m_width;
		const int& 				m_height;
		const size_t& 			m_size;
		Image& 					m_texture;
	};

	template<typename T = ImgCreateTextureImageInfo>
	inline void ImgCreateTextureImage(T&& info) {

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;
        BufCreateBuffer( {
			.m_vmaAllocator 	= info.m_vmaAllocator, 
			.m_size 			= info.m_size, 
			.m_usage 		= VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			.m_properties 		= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			.m_vmaFlags 		= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, 
			.m_buffer 			= stagingBuffer, 
			.m_allocation 		= stagingBufferAllocation, 
			.m_allocationInfo 	= &allocInfo
		});

        memcpy(allocInfo.pMappedData, info.m_pixels, info.m_size);

        ImgCreateImage2({
			info.m_physicalDevice, 
			info.m_device, 
			info.m_vmaAllocator, 
			(uint32_t)info.m_width, 
			(uint32_t)info.m_height, 
			VK_FORMAT_R8G8B8A8_SRGB, 
			VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			info.m_texture.m_mapImage,
			info.m_texture.m_mapImageAllocation
		}); 
		
        ImgTransitionImageLayout2({
			info.m_device, 
			info.m_graphicsQueue, 
			info.m_commandPool, 
			info.m_texture.m_mapImage, 
			VK_FORMAT_R8G8B8A8_SRGB, 
			VK_IMAGE_LAYOUT_UNDEFINED, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		});
    
        BufCopyBufferToImage({
			.m_device 			= info.m_device, 
			.m_graphicsQueue 	= info.m_graphicsQueue, 
			.m_commandPool 		= info.m_commandPool, 
			.m_buffer 			= stagingBuffer, 
			.m_image 			= info.m_texture.m_mapImage, 
			.m_width 			= static_cast<uint32_t>(info.m_width), 
			.m_height 			= static_cast<uint32_t>(info.m_height)
		});

        ImgTransitionImageLayout2({
			info.m_device, 
			info.m_graphicsQueue, 
			info.m_commandPool, 
			info.m_texture.m_mapImage, 
			VK_FORMAT_R8G8B8A8_SRGB, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});

        BufDestroyBuffer({info.m_device, info.m_vmaAllocator, stagingBuffer, stagingBufferAllocation});
    }

	//---------------------------------------------------------------------------------------------

    struct ImgDestroyImageInfo {
		const VkDevice& m_device;
		const VmaAllocator& m_vmaAllocator;
		const VkImage& m_image;
		const VmaAllocation& m_imageAllocation;
	};

	template<typename T = ImgDestroyImageInfo>
    inline void ImgDestroyImage(T&& info) {
        vmaDestroyImage(info.m_vmaAllocator, info.m_image, info.m_imageAllocation);
    }

	
	//---------------------------------------------------------------------------------------------

	struct ImgSwapChannelsInfo {
		unsigned char * m_bufferData; 
		int& m_r; 
		int& m_g; 
		int& m_b; 
		int& m_a; 
		int& m_width; 
		int& m_height;
	};

	template<typename T = ImgSwapChannelsInfo>
	inline void ImgSwapChannels(T&& info) {
		for (uint32_t i = 0; i < info.m_width * info.m_height; i++)
		{
			unsigned char rc = info.m_bufferData[4 * i + 0];
			unsigned char gc = info.m_bufferData[4 * i + 1];
			unsigned char bc = info.m_bufferData[4 * i + 2];
			unsigned char ac = info.m_bufferData[4 * i + 3];

			info.m_bufferData[4 * i + info.m_r] = rc;
			info.m_bufferData[4 * i + info.m_g] = gc;
			info.m_bufferData[4 * i + info.m_b] = bc;
			info.m_bufferData[4 * i + info.m_a] = ac;
		}
	}

	//---------------------------------------------------------------------------------------------

    struct ImgCopyImageToHostinfo {
		const VkDevice& 		m_device; 
		const VmaAllocator& 	m_vmaAllocator; 
		const VkQueue& 			m_graphicsQueue; 
    	const VkCommandPool& 	m_commandPool; 
		const VkImage& 			m_image; 
		const VkFormat& 		m_format; 
		const VkImageAspectFlagBits& m_aspects; 
		const VkImageLayout& 	m_layout;
    	unsigned char * 	m_bufferData; 
		const uint32_t& m_width; 
		const uint32_t&	m_height; 
		const uint32_t& m_size; 
		const int& 		m_r; 
		const int& 		m_g; 
		const int& 		m_b; 
		const int& 		m_a;
	};
    
	template<typename T = ImgCopyImageToHostinfo>
	inline auto ImgCopyImageToHost(T&& info) -> VkResult {

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;

        BufCreateBuffer( {
			.m_vmaAllocator = info.m_vmaAllocator, 
			.m_size = info.m_size, 
			.m_usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
			.m_properties = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.m_vmaFlags = VMA_MEMORY_USAGE_CPU_ONLY, 
            .m_buffer = stagingBuffer, 
			.m_allocation = stagingBufferAllocation, 
			.m_allocationInfo = &allocInfo
		});

		ImgTransitionImageLayout2({
			.m_device = info.m_device, 
			.m_graphicsQueue = info.m_graphicsQueue, 
			.m_commandPool = info.m_commandPool, 
			.m_image = info.m_image, 
			.m_format = info.m_format, 
			.m_oldLayout = info.m_layout, 
			.m_newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		});

		BufCopyImageToBuffer2({
			.m_device = info.m_device, 
			.m_graphicsQueue = info.m_graphicsQueue, 
			.m_commandPool = info.m_commandPool, 
			.m_image = info.m_image, 
			.m_aspectMask = info.m_aspects, 
			.m_buffer = stagingBuffer, 
			.m_layerCount = 1, 
			.m_width = info.m_width, 
			.m_height = info.m_height
		});

		ImgTransitionImageLayout2({
			.m_device = info.m_device, 
			.m_graphicsQueue = info.m_graphicsQueue, 
			.m_commandPool = info.m_commandPool, 
			.m_image = info.m_image, 
			.m_format = info.m_format, 
			.m_oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
			.m_newLayout = info.m_layout
		});

		void *data;
		vmaMapMemory( info.m_vmaAllocator, stagingBufferAllocation, &data);
		memcpy(info.m_bufferData, data, (size_t)info.m_size);
		vmaUnmapMemory( info.m_vmaAllocator, stagingBufferAllocation);

		vvh::ImgSwapChannels(info);

		vmaDestroyBuffer( info.m_vmaAllocator, stagingBuffer, stagingBufferAllocation);
		return VK_SUCCESS;
	}


}

