#include "VHInclude.h"

namespace vvh {

    /**
     * @brief Create a texture image from pixel data and upload it to GPU memory
     * @param physicalDevice Physical device for image creation
     * @param device Logical device for creating image and buffers
     * @param vmaAllocator VMA allocator for memory management
     * @param graphicsQueue Graphics queue for command submission
     * @param commandPool Command pool for allocating command buffers
     * @param pixels Pointer to pixel data in host memory
     * @param texWidth Width of the texture in pixels
     * @param texHeight Height of the texture in pixels
     * @param imageSize Size of the pixel data in bytes
     * @param texture Output texture object with image and allocation
     */
    void ImgCreateTextureImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator,
        VkQueue graphicsQueue, VkCommandPool commandPool, void* pixels, int texWidth, int texHeight,
        size_t imageSize, Image& texture) {

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;
        BufCreateBuffer( {
			vmaAllocator, 
			imageSize, 
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, 
			stagingBuffer, 
			stagingBufferAllocation, 
			&allocInfo
		});

        memcpy(allocInfo.pMappedData, pixels, imageSize);

        ImgCreateImage2({
			physicalDevice, 
			device, 
			vmaAllocator, 
			(uint32_t)texWidth, 
			(uint32_t)texHeight, 
			VK_FORMAT_R8G8B8A8_SRGB, 
			VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			VK_IMAGE_LAYOUT_UNDEFINED, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			texture.m_mapImage, 
			texture.m_mapImageAllocation
		}); 
		
        ImgTransitionImageLayout2({
			device, 
			graphicsQueue, 
			commandPool, 
			texture.m_mapImage, 
			VK_FORMAT_R8G8B8A8_SRGB, 
			VK_IMAGE_LAYOUT_UNDEFINED, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		});
    
        BufCopyBufferToImage({
			.m_device = device, 
			.m_graphicsQueue = graphicsQueue, 
			.m_commandPool = commandPool, 
			.m_buffer = stagingBuffer, 
			.m_image = texture.m_mapImage, 
			.m_width = static_cast<uint32_t>(texWidth), 
			.m_height = static_cast<uint32_t>(texHeight)
		});

        ImgTransitionImageLayout2({
			device, 
			graphicsQueue, 
			commandPool, 
			texture.m_mapImage, 
			VK_FORMAT_R8G8B8A8_SRGB, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});

        BufDestroyBuffer({device, vmaAllocator, stagingBuffer, stagingBufferAllocation});
    }

    /**
     * @brief Create an image view for a texture to enable shader access
     * @param device Logical device for creating the image view
     * @param texture Texture object to create image view for
     */
    void ImgCreateTextureImageView(VkDevice device, Image& texture) {
      	texture.m_mapImageView = ImgCreateImageView2({
			device, 
			texture.m_mapImage, 
			VK_FORMAT_R8G8B8A8_SRGB, 
			VK_IMAGE_ASPECT_COLOR_BIT
		});
    }

    /**
     * @brief Create a texture sampler with anisotropic filtering for texture sampling
     * @param physicalDevice Physical device for querying device properties
     * @param device Logical device for creating the sampler
     * @param texture Texture object to create sampler for
     */
    void ImgCreateTextureSampler(VkPhysicalDevice physicalDevice, VkDevice device, Image &texture) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

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

        if (vkCreateSampler(device, &samplerInfo, nullptr, &texture.m_mapSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    /**
     * @brief Create an image view for a Vulkan image with single layer and mip level
     * @param device Logical device for creating the image view
     * @param image Vulkan image to create view for
     * @param format Image format
     * @param aspectFlags Image aspect flags (color, depth, or stencil)
     * @return Created image view handle
     */
    VkImageView ImgCreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        return ImgCreateImageView({device, image, format, aspectFlags, 1, 1});
    }

    /**
     * @brief Create an image view for a Vulkan image with specified layers and mip levels
     * @param device Logical device for creating the image view
     * @param image Vulkan image to create view for
     * @param format Image format
     * @param aspectFlags Image aspect flags (color, depth, or stencil)
     * @param layers Number of array layers to include in the view
     * @param mipLevels Number of mipmap levels to include in the view
     * @return Created image view handle
     */
    VkImageView ImgCreateImageView2(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                    uint32_t layers, uint32_t mipLevels ) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = layers;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }

        return imageView;
    }

    /**
     * @brief Create a 2D Vulkan image with single layer and mip level
     * @param physicalDevice Physical device for image creation
     * @param device Logical device for creating the image
     * @param vmaAllocator VMA allocator for memory management
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param format Image format
     * @param tiling Image tiling mode (linear or optimal)
     * @param usage Image usage flags
     * @param imageLayout Initial image layout
     * @param properties Memory property flags
     * @param image Output image handle
     * @param imageAllocation Output VMA allocation handle
     */
    void ImgCreateImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, uint32_t width, uint32_t height
        , VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageLayout imageLayout
        , VkMemoryPropertyFlags properties, VkImage& image, VmaAllocation& imageAllocation) {

        return 	ImgCreateImage({
					physicalDevice, 
					device, 
					vmaAllocator, 
                    width, height, 
					1, 1, 1, 
                    format, 
					tiling, 
					usage, 
					imageLayout, 
					properties, 
                    image, 
					imageAllocation
				});
    }

    /**
     * @brief Create a Vulkan image with specified depth, layers, and mip levels
     * @param physicalDevice Physical device for image creation
     * @param device Logical device for creating the image
     * @param vmaAllocator VMA allocator for memory management
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param depth Image depth (typically 1 for 2D images)
     * @param layers Number of array layers
     * @param mipLevels Number of mipmap levels
     * @param format Image format
     * @param tiling Image tiling mode (linear or optimal)
     * @param usage Image usage flags
     * @param imageLayout Initial image layout
     * @param properties Memory property flags
     * @param image Output image handle
     * @param imageAllocation Output VMA allocation handle
     */
    void ImgCreateImage2(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , uint32_t width, uint32_t height, uint32_t depth, uint32_t layers, uint32_t mipLevels
        , VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageLayout imageLayout
        , VkMemoryPropertyFlags properties, VkImage& image, VmaAllocation& imageAllocation) {

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = depth;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = layers;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = imageLayout;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocInfo.priority = 1.0f;
        vmaCreateImage(vmaAllocator, &imageInfo, &allocInfo, &image, &imageAllocation, nullptr);
    }

    /**
     * @brief Create an image sampler with nearest filtering and clamp-to-edge addressing
     * @param physicalDevice Physical device for querying device properties
     * @param device Logical device for creating the sampler
     * @param sampler Output sampler handle
     */
    void ImgCreateImageSampler(VkPhysicalDevice physicalDevice, VkDevice device, VkSampler& sampler) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image sampler!");
        }
    }

    /**
     * @brief Destroy a Vulkan image and free its allocated memory
     * @param device Logical device (unused but kept for API consistency)
     * @param vmaAllocator VMA allocator used to create the image
     * @param image Image handle to destroy
     * @param imageAllocation VMA allocation handle to free
     */
    void ImgDestroyImage(VkDevice device, VmaAllocator vmaAllocator, VkImage image, VmaAllocation& imageAllocation) {
        vmaDestroyImage(vmaAllocator, image, imageAllocation);
    }

    /**
     * @brief Transition an image from one layout to another using a pipeline barrier
     * @param device Logical device for command buffer operations
     * @param graphicsQueue Graphics queue for command submission
     * @param commandPool Command pool for allocating command buffers
     * @param image Image to transition
     * @param format Image format
     * @param oldLayout Current image layout
     * @param newLayout Target image layout
     */
    void ImgTransitionImageLayout(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
            ImgTransitionImageLayout({
				device, 
				graphicsQueue, 
				commandPool, 
				image, 
				format, 
				VK_IMAGE_ASPECT_COLOR_BIT, 
				1, 1, 
				oldLayout, 
				newLayout
			});
    }

    /**
     * @brief Transition an image layout with specified aspect, mip levels, and layers
     * @param device Logical device for command buffer operations
     * @param graphicsQueue Graphics queue for command submission
     * @param commandPool Command pool for allocating command buffers
     * @param image Image to transition
     * @param format Image format
     * @param aspect Image aspect flags (color, depth, or stencil)
     * @param numMipLevels Number of mip levels to transition
     * @param numLayers Number of array layers to transition
     * @param oldLayout Current image layout
     * @param newLayout Target image layout
     */
    void ImgTransitionImageLayout2(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
         , VkImage image, VkFormat format
         , VkImageAspectFlags aspect, int numMipLevels, int numLayers
         , VkImageLayout oldLayout, VkImageLayout newLayout) {

        VkCommandBuffer commandBuffer = ComBeginSingleTimeCommands({device, commandPool});

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = numMipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = numLayers;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if(oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        } else if(oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;
            sourceStage = 0;
            destinationStage = 0;
        } else if(oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
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
              sourceStage, destinationStage,
              0,
              0, nullptr,
              0, nullptr,
              1, &barrier
        );

        ComEndSingleTimeCommands({device, graphicsQueue, commandPool, commandBuffer});
    }


	/**
	* \brief Copy a swap chain image to a data buffer, after it has been rendered into
	*
	* \param[in] device Logical Vulkan device
	* \param[in] allocator VMA allocator
	* \param[in] graphicsQueue Device queue for submitting commands
	* \param[in] commandPool Command pool for allocating command buffers
	* \param[in] image The source image
	* \param[in] format Format of the image
	* \param[in] aspect Color or depth
	* \param[in] layout The layout the image is currently in
	* \param[in] bufferData The destination buffer data
	* \param[in] width �mage width
	* \param[in] height Image height
	* \param[in] imageSize Size of the image in bytes
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult ImgCopyImageToHost(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, 
        VkCommandPool commandPool, VkImage image, VkFormat format, VkImageAspectFlagBits aspect, VkImageLayout layout,
		/*gli::byte*/ unsigned char *bufferData, uint32_t width, uint32_t height, uint32_t imageSize, int r, int g, int b, int a)
	{
		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;

        BufCreateBuffer({
			.m_vmaAllocator = allocator, 
			.m_size = (VkDeviceSize)imageSize, 
			.m_usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            .m_properties = VMA_MEMORY_USAGE_CPU_ONLY, 
			.m_vmaFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .m_buffer = stagingBuffer, 
			.m_allocation = stagingBufferAllocation, 
			.m_allocationInfo = &allocInfo
		});

		ImgTransitionImageLayout(device, graphicsQueue, commandPool, image, format, 
			//aspect, 1, 1, 
			layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		BufCopyImageToBuffer2({
			.m_device = device, 
			.m_graphicsQueue = graphicsQueue, 
			.m_commandPool = commandPool, 
			.m_image = image, 
			.m_aspectMask = aspect, 
			.m_buffer = stagingBuffer, 
			.m_layerCount = 1, 
			.m_width = width, 
			.m_height = height
		});

		ImgTransitionImageLayout(device, graphicsQueue, commandPool, image, format, 
			//aspect, 1, 1, 
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout);

		void *data;
		vmaMapMemory(allocator, stagingBufferAllocation, &data);
		memcpy(bufferData, data, (size_t)imageSize);
		vmaUnmapMemory(allocator, stagingBufferAllocation);

		ImgSwapChannels({
			.m_bufferData = bufferData, 
			.m_r = r, 
			.m_g = g, 
			.m_b = b, 
			.m_a = a, 
			.m_width = (int)width, 
			.m_height = (int)height
		});

		vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
		return VK_SUCCESS;
	}

	/**
	 * @brief Swap color channels in image buffer data to reorder RGBA components
	 * @param bufferData Pointer to image buffer data
	 * @param r Target index for red channel
	 * @param g Target index for green channel
	 * @param b Target index for blue channel
	 * @param a Target index for alpha channel
	 * @param width Image width in pixels
	 * @param height Image height in pixels
	 */
	void ImgSwapChannels(unsigned char *bufferData, int r, int g, int b, int a, int width, int height) {
		for (uint32_t i = 0; i < width * height; i++)
		{
			unsigned char rc = bufferData[4 * i + 0];
			unsigned char gc = bufferData[4 * i + 1];
			unsigned char bc = bufferData[4 * i + 2];
			unsigned char ac = bufferData[4 * i + 3];

			bufferData[4 * i + r] = rc;
			bufferData[4 * i + g] = gc;
			bufferData[4 * i + b] = bc;
			bufferData[4 * i + a] = ac;
		}
	}

	/**
	 * @brief Select a suitable depth map format from candidates based on device support
	 * @param physicalDevice Physical device to query for format support
	 * @param depthFormats Vector of candidate depth formats to check
	 * @param depthMapFormat Output parameter for selected depth format
	 */
    void ImgPickDepthMapFormat( VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& depthFormats, VkFormat& depthMapFormat ) {
        for( auto format : depthFormats ) {
            VkFormatProperties properties;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
            if( properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT &&
                properties.linearTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
                ) {
                depthMapFormat = format;
                break;
            }
        }
    }

}

