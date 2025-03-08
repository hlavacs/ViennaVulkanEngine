#include "VHInclude.h"

namespace vh {

    void ImgCreateTextureImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, 
        VkQueue graphicsQueue, VkCommandPool commandPool, void* pixels, int texWidth, int texHeight, 
        size_t imageSize, vh::Map& texture) {

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;
        BufCreateBuffer( vmaAllocator, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            , VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            , stagingBuffer, stagingBufferAllocation, &allocInfo);

        memcpy(allocInfo.pMappedData, pixels, imageSize);

        ImgCreateImage(physicalDevice, device, vmaAllocator, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB
            , VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.m_mapImage, texture.m_mapImageAllocation); 

        ImgTransitionImageLayout(device, graphicsQueue, commandPool, texture.m_mapImage, VK_FORMAT_R8G8B8A8_SRGB
            , VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        BufCopyBufferToImage(device, graphicsQueue, commandPool, stagingBuffer, texture.m_mapImage
                 , static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        ImgTransitionImageLayout(device, graphicsQueue, commandPool, texture.m_mapImage, VK_FORMAT_R8G8B8A8_SRGB
            , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        BufDestroyBuffer(device, vmaAllocator, stagingBuffer, stagingBufferAllocation);
    }

    void ImgCreateTextureImageView(VkDevice device, Map& texture) {
      texture.m_mapImageView = ImgCreateImageView(device, texture.m_mapImage, VK_FORMAT_R8G8B8A8_SRGB
                                      , VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void ImgCreateTextureSampler(VkPhysicalDevice physicalDevice, VkDevice device, Map &texture) {
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

    VkImageView ImgCreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }

        return imageView;
    }

    void ImgCreateImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, uint32_t width, uint32_t height
        , VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties
        , VkImage& image, VmaAllocation& imageAllocation) {

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        allocInfo.priority = 1.0f;
        vmaCreateImage(vmaAllocator, &imageInfo, &allocInfo, &image, &imageAllocation, nullptr);
    }

    void ImgDestroyImage(VkDevice device, VmaAllocator vmaAllocator, VkImage image, VmaAllocation& imageAllocation) {
        vmaDestroyImage(vmaAllocator, image, imageAllocation);
    }

    void ImgTransitionImageLayout(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
         , VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {

        VkCommandBuffer commandBuffer = ComBeginSingleTimeCommands(device, commandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

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
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
              commandBuffer,
              sourceStage, destinationStage,
              0,
              0, nullptr,
              0, nullptr,
              1, &barrier
        );

        ComEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer);
    }


    /**
	* \brief Copy a swap chain image to a data buffer, after it has been rendered into
	*
	* \param[in] device Logical Vulkan device
	* \param[in] allocator VMA allocator
	* \param[in] graphicsQueue Device queue for submitting commands
	* \param[in] commandPool Command pool for allocating command buffers
	* \param[in] image The source image
	* \param[in] format The pixel format of this image
	* \param[in] aspect Color or depth
	* \param[in] layout The layout that this image is currently and should be again after the copy
	* \param[in] bufferData The destination buffer data
	* \param[in] width �mage width
	* \param[in] height Image height
	* \param[in] imageSize Size of the image in bytes
	* \returns VK_SUCCESS or a Vulkan error code
	*
	*/
	VkResult ImgCopySwapChainImageToHost(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, 
        VkCommandPool commandPool, VkImage image, VkFormat format, VkImageAspectFlagBits aspect, VkImageLayout layout,
		/*gli::byte*/ unsigned char *bufferData, uint32_t width, uint32_t height, uint32_t imageSize) 
	{
		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;

        BufCreateBuffer(allocator, (VkDeviceSize)imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            stagingBuffer, stagingBufferAllocation, &allocInfo);

		ImgTransitionImageLayout(device, graphicsQueue, commandPool, image, format, 
            //aspect, 1, 1, 
            layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		BufCopyImageToBuffer(device, graphicsQueue, commandPool, image, aspect , stagingBuffer, 1, width, height);

		ImgTransitionImageLayout(device, graphicsQueue, commandPool, image, format, 
            //aspect, 1, 1, 
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout);

		void *data;
		vmaMapMemory(allocator, stagingBufferAllocation, &data);
		memcpy(bufferData, data, (size_t)imageSize);
		vmaUnmapMemory(allocator, stagingBufferAllocation);

		for (uint32_t i = 0; i < width * height; i++)
		{
			/*gli::byte*/ unsigned char r = bufferData[4 * i + 0];
			/*gli::byte*/ unsigned char g = bufferData[4 * i + 1];
			/*gli::byte*/ unsigned char b = bufferData[4 * i + 2];
			/*gli::byte*/ unsigned char a = bufferData[4 * i + 3];

			bufferData[4 * i + 0] = b;
			bufferData[4 * i + 1] = g;
			bufferData[4 * i + 2] = r;
			bufferData[4 * i + 3] = a;
		}

		vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
		return VK_SUCCESS;
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
		/*gli::byte*/ unsigned char *bufferData, uint32_t width, uint32_t height, uint32_t imageSize)
	{
		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;

        BufCreateBuffer(allocator, (VkDeviceSize)imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            VMA_MEMORY_USAGE_CPU_ONLY, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            stagingBuffer, stagingBufferAllocation, &allocInfo);

		//ImgTransitionImageLayout(device, graphicsQueue, commandPool, image, format, aspect, 1, 1, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		//ImgCopyImageToBuffer(device, graphicsQueue, commandPool, image, aspect, stagingBuffer, 1, width, height);

		//ImgTransitionImageLayout(device, graphicsQueue, commandPool, image, format, aspect, 1, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout);

		void *data;
		vmaMapMemory(allocator, stagingBufferAllocation, &data);
		memcpy(bufferData, data, (size_t)imageSize);
		vmaUnmapMemory(allocator, stagingBufferAllocation);

		vmaDestroyBuffer(allocator, stagingBuffer, stagingBufferAllocation);
		return VK_SUCCESS;
	}


}

