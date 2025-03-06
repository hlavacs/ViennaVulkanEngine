#include "VHInclude.h"

namespace vh {

    void BufCreateBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
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

    void BufDestroyBuffer(VkDevice device, VmaAllocator vmaAllocator, VkBuffer buffer, VmaAllocation& allocation) {
        vmaDestroyBuffer(vmaAllocator, buffer, allocation);
    }

    void BufDestroyBuffer2(VkDevice device, VmaAllocator vmaAllocator, UniformBuffers buffers) {
		for (size_t i = 0; i < buffers.m_uniformBuffers.size(); i++) {
			vmaDestroyBuffer(vmaAllocator, buffers.m_uniformBuffers[i], buffers.m_uniformBuffersAllocation[i]);
		}
	}

    void BufCopyBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkBuffer srcBuffer
        , VkBuffer dstBuffer, VkDeviceSize size) {

        VkCommandBuffer commandBuffer = ComBeginSingleTimeCommands(device, commandPool);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        ComEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer);
    }


    void BufCopyBufferToImage(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {

        VkCommandBuffer commandBuffer = ComBeginSingleTimeCommands(device, commandPool);

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

        ComEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer);
    }


    void BufCreateVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Mesh& geometry) {

        VkDeviceSize bufferSize = geometry.m_verticesData.getSize();

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;
        BufCreateBuffer(physicalDevice, device, vmaAllocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
            , VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            , stagingBuffer, stagingBufferAllocation, &allocInfo);

		geometry.m_verticesData.copyData( allocInfo.pMappedData );
	
        BufCreateBuffer(physicalDevice, device, vmaAllocator, bufferSize
            , VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
            , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, geometry.m_vertexBuffer
            , geometry.m_vertexBufferAllocation);

        BufCopyBuffer(device, graphicsQueue, commandPool, stagingBuffer, geometry.m_vertexBuffer, bufferSize);

        BufDestroyBuffer(device, vmaAllocator, stagingBuffer, stagingBufferAllocation);
    }


    void BufCreateIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Mesh& geometry) {

        VkDeviceSize bufferSize = sizeof(geometry.m_indices[0]) * geometry.m_indices.size();

        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        VmaAllocationInfo allocInfo;
        BufCreateBuffer(physicalDevice, device, vmaAllocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT
            , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
            , VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
            , stagingBuffer, stagingBufferAllocation, &allocInfo);

		memcpy(allocInfo.pMappedData, geometry.m_indices.data(), bufferSize);

        BufCreateBuffer(physicalDevice, device, vmaAllocator, bufferSize
            , VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
            , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0
            , geometry.m_indexBuffer, geometry.m_indexBufferAllocation);

        BufCopyBuffer(device, graphicsQueue, commandPool, stagingBuffer, geometry.m_indexBuffer, bufferSize);

        BufDestroyBuffer(device, vmaAllocator, stagingBuffer, stagingBufferAllocation);
    }

    void BufCreateUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator& vmaAllocator, 
		VkDeviceSize bufferSize, UniformBuffers &uniformBuffers) {

		uniformBuffers.m_bufferSize = bufferSize;
        uniformBuffers.m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffers.m_uniformBuffersAllocation.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffers.m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VmaAllocationInfo allocInfo;
            BufCreateBuffer(physicalDevice, device, vmaAllocator, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                , VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
                , VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
                , uniformBuffers.m_uniformBuffers[i] 
                , uniformBuffers.m_uniformBuffersAllocation[i], &allocInfo);

            uniformBuffers.m_uniformBuffersMapped[i] = allocInfo.pMappedData;
        }
    }




} // namespace vh

