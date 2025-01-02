
#include "VHInclude.h"

namespace vh {

    void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties
        , VmaAllocationCreateFlags vmaFlags, VkBuffer& buffer
        , VmaAllocation& allocation, VmaAllocationInfo* allocationInfo) {
    
        VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = vmaFlags;
        vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocInfo, &buffer, &allocation, allocationInfo);
    }

    void updateUniformBuffer(uint32_t currentImage, SwapChain& swapChain, UniformBuffers& uniformBuffers) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

		/*vec3_t m_position{0.0, 0.0, 0.0};
		quat_t m_rotation{};
		vec3_t m_scale{1.0, 1.0, 1.0};
		mat4_t m_matrix{1.0};
		auto Matrix = [&]()-> mat4_t { m_matrix = glm::translate(mat4_t{}, m_position) * glm::mat4_cast(m_rotation) * glm::scale(mat4_t{}, m_scale); return m_matrix; };
		auto view = glm::inverse( Matrix() );
		*/
	

        ubo.proj = glm::perspective(glm::radians(45.0f)
            , swapChain.m_swapChainExtent.width / (float) swapChain.m_swapChainExtent.height, 0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        memcpy(uniformBuffers.m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }


    void destroyBuffer(VkDevice device, VmaAllocator vmaAllocator, VkBuffer buffer, VmaAllocation& allocation) {
        vmaDestroyBuffer(vmaAllocator, buffer, allocation);
    }

    void destroyBuffer2(VkDevice device, VmaAllocator vmaAllocator, UniformBuffers buffers) {
		for (size_t i = 0; i < buffers.m_uniformBuffers.size(); i++) {
			vmaDestroyBuffer(vmaAllocator, buffers.m_uniformBuffers[i], buffers.m_uniformBuffersAllocation[i]);
		}
	}

    void copyBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkBuffer srcBuffer
        , VkBuffer dstBuffer, VkDeviceSize size) {

        VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer);
    }


  	void createTextureImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, 
  			VkQueue graphicsQueue, VkCommandPool commandPool, void* pixels, int texWidth, int texHeight, 
			size_t imageSize, vh::Texture& texture) {

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;
        createBuffer(physicalDevice, device, vmaAllocator, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            , VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            , stagingBuffer, stagingBufferAllocation, &allocInfo);

        MemCopy(device, pixels, allocInfo, imageSize);

        createImage(physicalDevice, device, vmaAllocator, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB
            , VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture.m_textureImage, texture.m_textureImageAllocation); 

        transitionImageLayout(device, graphicsQueue, commandPool, texture.m_textureImage, VK_FORMAT_R8G8B8A8_SRGB
            , VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(device, graphicsQueue, commandPool, stagingBuffer, texture.m_textureImage
                , static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        transitionImageLayout(device, graphicsQueue, commandPool, texture.m_textureImage, VK_FORMAT_R8G8B8A8_SRGB
            , VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        destroyBuffer(device, vmaAllocator, stagingBuffer, stagingBufferAllocation);
    }




    void createTextureImageView(VkDevice device, Texture& texture) {
        texture.m_textureImageView = createImageView(device, texture.m_textureImage, VK_FORMAT_R8G8B8A8_SRGB
                                        , VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createTextureSampler(VkPhysicalDevice physicalDevice, VkDevice device, Texture &texture) {
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

        if (vkCreateSampler(device, &samplerInfo, nullptr, &texture.m_textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
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


    void createImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, uint32_t width, uint32_t height
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

    void destroyImage(VkDevice device, VmaAllocator vmaAllocator, VkImage image, VmaAllocation& imageAllocation) {
        vmaDestroyImage(vmaAllocator, image, imageAllocation);
    }


    void transitionImageLayout(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {

        VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

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
		}
		else {
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

        endSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer);
    }

    void copyBufferToImage(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {

        VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer);
    }


    void createVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Geometry& geometry) {

        VkDeviceSize bufferSize = sizeof(geometry.m_vertices[0]) * geometry.m_vertices.size();

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;
        createBuffer(physicalDevice, device, vmaAllocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
            , VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            , stagingBuffer, stagingBufferAllocation, &allocInfo);

        MemCopy(device, geometry.m_vertices.data(), allocInfo, bufferSize);

        createBuffer(physicalDevice, device, vmaAllocator, bufferSize
            , VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, geometry.m_vertexBuffer
            , geometry.m_vertexBufferAllocation);

        copyBuffer(device, graphicsQueue, commandPool, stagingBuffer, geometry.m_vertexBuffer, bufferSize);

        destroyBuffer(device, vmaAllocator, stagingBuffer, stagingBufferAllocation);
    }

    void createIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Geometry& geometry) {

        VkDeviceSize bufferSize = sizeof(geometry.m_indices[0]) * geometry.m_indices.size();

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;
        createBuffer(physicalDevice, device, vmaAllocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
            , VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            , stagingBuffer, stagingBufferAllocation, &allocInfo);

        MemCopy(device, geometry.m_indices.data(), allocInfo, bufferSize);

        createBuffer(physicalDevice, device, vmaAllocator, bufferSize
            , VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0
            , geometry.m_indexBuffer, geometry.m_indexBufferAllocation);

        copyBuffer(device, graphicsQueue, commandPool, stagingBuffer, geometry.m_indexBuffer, bufferSize);

        destroyBuffer(device, vmaAllocator, stagingBuffer, stagingBufferAllocation);
    }

    void createUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator& vmaAllocator
            , UniformBuffers &uniformBuffers) {

        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffers.m_uniformBuffersAllocation.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffers.m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VmaAllocationInfo allocInfo;
            createBuffer(physicalDevice, device, vmaAllocator, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
                , VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
                , uniformBuffers.m_uniformBuffers[i] 
                , uniformBuffers.m_uniformBuffersAllocation[i], &allocInfo);

            uniformBuffers.m_uniformBuffersMapped[i] = allocInfo.pMappedData;
        }
    }




} // namespace vh

