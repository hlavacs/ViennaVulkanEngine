#pragma once


namespace vvh {

	struct ComEndSingleTimeCommandsInfo {
		const VkDevice& m_device;
		const VkQueue& m_graphicsQueue;
		const VkCommandPool& m_commandPool;
		const VkCommandBuffer& m_commandBuffer;
	};

	template<typename T = ComEndSingleTimeCommandsInfo>
	void ComEndSingleTimeCommands(T&& info);
	
	//---------------------------------------------------------------------------------------------

    struct BufCreateBufferInfo { 
		const VmaAllocator& 			m_vmaAllocator;
        const VkDeviceSize& 			m_size;
		const VkBufferUsageFlags& 		m_usageFlags;
		const VkMemoryPropertyFlags& 	m_properties;
        const VmaAllocationCreateFlags& m_vmaFlags;
		VkBuffer& 						m_buffer;
        VmaAllocation& 					m_allocation;
		VmaAllocationInfo* 				m_allocationInfo;
	};

	template<typename T = BufCreateBufferInfo>
	inline void BufCreateBuffer(T&& info) {
		VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.size = info.m_size;
		bufferInfo.usage = info.m_usageFlags;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = info.m_vmaFlags;
		vmaCreateBuffer(info.m_vmaAllocator, &bufferInfo, &allocInfo, &info.m_buffer, &info.m_allocation, info.m_allocationInfo);
	}

	//---------------------------------------------------------------------------------------------

    struct BufCreateBuffersInfo {
		const VkDevice& 			m_device;
		const VmaAllocator& 		m_vmaAllocator;
        const VkBufferUsageFlags& 	m_usageFlags;
		const VkDeviceSize& 		m_size;
		Buffer& 					m_buffer;
	};
    
	template<typename T = BufCreateBuffersInfo>
	inline void BufCreateBuffers(T&& info) {

		info.m_buffer.m_bufferSize = info.m_size;
		info.m_buffer.m_uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		info.m_buffer.m_uniformBuffersAllocation.resize(MAX_FRAMES_IN_FLIGHT);
		info.m_buffer.m_uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);
	
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			VmaAllocationInfo allocInfo;
			BufCreateBuffer( {
				.m_vmaAllocator = info.m_vmaAllocator,
				.m_size 		= info.m_size, 
				.m_usageFlags 	= info.m_usageFlags, 
				.m_properties 	= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				.m_vmaFlags 	= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
				.m_buffer 		= info.m_buffer.m_uniformBuffers[i],
				.m_allocation 	= info.m_buffer.m_uniformBuffersAllocation[i],
				.m_allocationInfo = &allocInfo
			});

			info.m_buffer.m_uniformBuffersMapped[i] = allocInfo.pMappedData;
		}    
	}

	//---------------------------------------------------------------------------------------------
    struct BufDestroyBufferinfo {
		const VkDevice& 		m_device;
		const VmaAllocator& 	m_vmaAllocator;
		const VkBuffer& 		m_buffer;
		const VmaAllocation& 	m_allocation;
	};

	template<typename T = BufDestroyBufferinfo>
	inline void BufDestroyBuffer(T&& info) {
        vmaDestroyBuffer(info.m_vmaAllocator, info.m_buffer, info.m_allocation);
    }

	//---------------------------------------------------------------------------------------------

    struct BufDestroyBuffer2Info {
		const VkDevice& 	m_device;
		const VmaAllocator& m_vmaAllocator;
		Buffer& 		m_buffers;
	};

	template<typename T = BufDestroyBuffer2Info>
	inline void BufDestroyBuffer2(T&& info) {
		for (size_t i = 0; i < info.m_buffers.m_uniformBuffers.size(); i++) {
			vmaDestroyBuffer(info.m_vmaAllocator, info.m_buffers.m_uniformBuffers[i], info.m_buffers.m_uniformBuffersAllocation[i]);
		}
	}
	//---------------------------------------------------------------------------------------------
    
	struct BufCopyBufferInfo {
		const VkDevice& 		m_device;
		const VkQueue& 			m_graphicsQueue;
		const VkCommandPool& 	m_commandPool;
		const VkBuffer& 		m_srcBuffer;
		const VkBuffer& 		m_dstBuffer;
		const VkDeviceSize& 	m_size;
	};

	template<typename T = BufCopyBufferInfo>
	inline void BufCopyBuffer(T&& info) {
        VkCommandBuffer commandBuffer = ComBeginSingleTimeCommands(info);
        VkBufferCopy copyRegion{};
        copyRegion.size = info.m_size;
        vkCmdCopyBuffer( commandBuffer, info.m_srcBuffer, info.m_dstBuffer, 1, &copyRegion );
        ComEndSingleTimeCommands({info.m_device, info.m_graphicsQueue, info.m_commandPool, commandBuffer});
    }

	//---------------------------------------------------------------------------------------------

    struct BufCopyBufferToImageInfo {
		const VkDevice& 		m_device;
		const VkQueue& 			m_graphicsQueue;
		const VkCommandPool& 	m_commandPool;
		const VkBuffer& 		m_buffer;
		const VkImage& 			m_image;
		const uint32_t& 		m_width;
		const uint32_t& 		m_height;
	};

	template<typename T = BufCopyBufferToImageInfo>
	void BufCopyBufferToImage(T&& info) {
		VkCommandBuffer commandBuffer = ComBeginSingleTimeCommands(info);
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = {0, 0, 0};
		region.imageExtent = {info.m_width, info.m_height, 1};

		vkCmdCopyBufferToImage(commandBuffer, info.m_buffer, info.m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		ComEndSingleTimeCommands({
			info.m_device, 
			info.m_graphicsQueue, 
			info.m_commandPool, 
			commandBuffer
		});
	}
	
	//---------------------------------------------------------------------------------------------

	struct BufCopyImageToBufferInfo {
		const VkDevice& 		m_device;
		const VkQueue& 			m_graphicsQueue;
		const VkCommandPool& 	m_commandPool;
		const VkImage& 			m_image;
		const VkBuffer& 		m_buffer;
		const std::vector<VkBufferImageCopy>& m_regions;
		const uint32_t& 		m_width;
		const uint32_t& 		m_height;
	};

	template<typename T = BufCopyImageToBufferInfo>
	inline void BufCopyImageToBuffer(T&& info) {
		VkCommandBuffer commandBuffer = ComBeginSingleTimeCommands(info);
		vkCmdCopyImageToBuffer(commandBuffer, info.m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, info.m_buffer, 
			(uint32_t)info.m_regions.size(), info.m_regions.data());
		ComEndSingleTimeCommands({info.m_device, info.m_graphicsQueue, info.m_commandPool, commandBuffer});
	}
	
	//---------------------------------------------------------------------------------------------
    
	struct BufCopyImageToBuffer2Info {
		const VkDevice& 		m_device;
		const VkQueue& 			m_graphicsQueue;
		const VkCommandPool& 	m_commandPool;
		const VkImage& 			m_image;
		const VkImageAspectFlagBits& m_aspectMask;
		const VkBuffer& 		m_buffer;
		const uint32_t& 		m_layerCount;
		const uint32_t& 		m_width;
		const uint32_t& 		m_height;
	};

	template<typename T = BufCopyImageToBuffer2Info>
	inline void BufCopyImageToBuffer2(T&& info) {
		std::vector<VkBufferImageCopy> regions;

		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = info.m_aspectMask;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = info.m_layerCount;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { info.m_width, info.m_height, 1 };
		regions.push_back(region);

		BufCopyImageToBuffer({
			info.m_device, 
			info.m_graphicsQueue, 
			info.m_commandPool, 
			info.m_image, 
			info.m_buffer, 
			regions, 
			info.m_width, 
			info.m_height
		});
	}


	//---------------------------------------------------------------------------------------------

    struct BufCreateVertexBufferInfo { 
		const VkPhysicalDevice& m_physicalDevice;
		const VkDevice& 		m_device;
		const VmaAllocator& 	m_vmaAllocator;
        const VkQueue&			m_graphicsQueue;
		const VkCommandPool& 	m_commandPool;
		Mesh& 					m_mesh;
	};

	template<typename T = BufCreateVertexBufferInfo>
	void BufCreateVertexBuffer(T&& info) {

		VkDeviceSize bufferSize = info.m_mesh.m_verticesData.getSize();

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation;
		VmaAllocationInfo allocInfo;
		BufCreateBuffer( {
			.m_vmaAllocator = info.m_vmaAllocator, 
			.m_size = bufferSize, 
			.m_usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			.m_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			.m_vmaFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, 
			.m_buffer = stagingBuffer, 
			.m_allocation = stagingBufferAllocation, 
			.m_allocationInfo = &allocInfo
		});

		info.m_mesh.m_verticesData.copyData( allocInfo.pMappedData );
	
		BufCreateBuffer( {
			.m_vmaAllocator = info.m_vmaAllocator, 
			.m_size = bufferSize, 
			.m_usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
			.m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			.m_vmaFlags = 0, 
			.m_buffer = info.m_mesh.m_vertexBuffer, 
			.m_allocation = info.m_mesh.m_vertexBufferAllocation
		});

		BufCopyBuffer( {info.m_device, info.m_graphicsQueue, info.m_commandPool, stagingBuffer, info.m_mesh.m_vertexBuffer, bufferSize });

		BufDestroyBuffer( {info.m_device, info.m_vmaAllocator, stagingBuffer, stagingBufferAllocation });
	}
	
	//---------------------------------------------------------------------------------------------

    struct BufCreateIndexBufferinfo { 
		const VkPhysicalDevice& m_physicalDevice;
		const VkDevice& 		m_device;
		const VmaAllocator& 	m_vmaAllocator;
		const VkQueue& 			m_graphicsQueue;
		const VkCommandPool& 	m_commandPool;
		Mesh& 					m_mesh;
	};

	template<typename T = BufCreateIndexBufferinfo>
	void BufCreateIndexBuffer(T&& info) {

		VkDeviceSize bufferSize = sizeof(info.m_mesh.m_indices[0]) * info.m_mesh.m_indices.size();

		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation;
		VmaAllocationInfo allocInfo;
		BufCreateBuffer( {
			.m_vmaAllocator = info.m_vmaAllocator, 
			.m_size = bufferSize, 
			.m_usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			.m_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			.m_vmaFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, 
			.m_buffer = stagingBuffer, 
			.m_allocation = stagingBufferAllocation, 
			.m_allocationInfo = &allocInfo
		});

		memcpy(allocInfo.pMappedData, info.m_mesh.m_indices.data(), bufferSize);

		BufCreateBuffer( {
			.m_vmaAllocator = info.m_vmaAllocator, 
			.m_size = bufferSize, 
			.m_usageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 
			.m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			.m_vmaFlags = 0, 
			.m_buffer = info.m_mesh.m_indexBuffer, 
			.m_allocation = info.m_mesh.m_indexBufferAllocation
		});

		BufCopyBuffer( {info.m_device, info.m_graphicsQueue, info.m_commandPool, stagingBuffer, info.m_mesh.m_indexBuffer, bufferSize} );

		BufDestroyBuffer( {info.m_device, info.m_vmaAllocator, stagingBuffer, stagingBufferAllocation});
	}
	

}; // namespace vh