/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/

#include "VHHelper.h"
#include "VKHelpers.h"

namespace vh
{
	/*
		Create a scratch buffer to hold temporary data for a ray tracing acceleration structure
		*/
	vhRayTracingScratchBuffer vhCreateScratchBuffer(VkDevice device, VmaAllocator vmaAllocator, VkDeviceSize size)
	{
		vhRayTracingScratchBuffer scratchBuffer{};

		vh::vhBufCreateBuffer(vmaAllocator, size,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU, &scratchBuffer.buffer, &scratchBuffer.allocation);
		scratchBuffer.deviceAddress = vhGetBufferDeviceAddress(device, scratchBuffer.buffer);

		vmaMapMemory(vmaAllocator, scratchBuffer.allocation, &scratchBuffer.mapped);

		return scratchBuffer;
	}

	void vhDeleteScratchBuffer(VmaAllocator vmaAllocator,
		vhRayTracingScratchBuffer &scratchBuffer)
	{
		vmaUnmapMemory(vmaAllocator, scratchBuffer.allocation);
		vmaDestroyBuffer(vmaAllocator, scratchBuffer.buffer, scratchBuffer.allocation);
	}

	VkResult vhCreateAccelerationStructureKHR(VmaAllocator vmaAllocator,
		vhAccelerationStructure &accelerationStructure,
		VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
	{
		return vh::vhBufCreateBuffer(vmaAllocator, buildSizeInfo.accelerationStructureSize,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			&accelerationStructure.resultBuffer,
			&accelerationStructure.resultBufferAllocation);
	}

	void vhDestroyAccelerationStructure(VkDevice device, VmaAllocator vmaAllocator, vhAccelerationStructure &as)
	{
		if (as.scratchMem)
		{
			vkDestroyBuffer(device, as.scratchBuffer, nullptr);
			vkFreeMemory(device, as.scratchMem, nullptr);
			as.scratchBuffer = VK_NULL_HANDLE;
		}
		if (as.resultMem)
		{
			vkDestroyBuffer(device, as.resultBuffer, nullptr);
			vkFreeMemory(device, as.resultMem, nullptr);
			as.resultBuffer = VK_NULL_HANDLE;
		}
		if (as.instancesMem)
		{
			vkDestroyBuffer(device, as.instancesBuffer, nullptr);
			vkFreeMemory(device, as.instancesMem, nullptr);
			as.instancesBuffer = VK_NULL_HANDLE;
		}

		if (as.resultBuffer)
			vmaDestroyBuffer(vmaAllocator, as.resultBuffer, as.resultBufferAllocation);
		if (as.instancesBuffer)
			vmaDestroyBuffer(vmaAllocator, as.instancesBuffer, as.instancesBufferAllocation);
		if (as.handleKHR)
			vkDestroyAccelerationStructureKHR(device, as.handleKHR, nullptr);
		if (as.handleNV)
			vkDestroyAccelerationStructureNV(device, as.handleNV, nullptr);
	}

	/*
		Gets the device address from a buffer that's required for some of the buffers used for ray tracing
	*/
	uint64_t vhGetBufferDeviceAddress(VkDevice device, VkBuffer buffer)
	{
		VkBufferDeviceAddressInfo bufferDeviceAI{};
		bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		bufferDeviceAI.buffer = buffer;
		return vkGetBufferDeviceAddress(device, &bufferDeviceAI);
	}

	//--------------------------------------------------------------------------------------------------
	//
	// Create a bottom-level acceleration structure based on a list of vertex
	// buffers in GPU memory along with their vertex count. The build is then done
	// in 3 steps: gathering the geometry, computing the sizes of the required
	// buffers, and building the actual acceleration structure #VKRay
	VkResult
		vhCreateBottomLevelAccelerationStructureKHR(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, vhAccelerationStructure &blas, const VkBuffer vertexBuffer, uint32_t vertexCount, const VkBuffer indexBuffer, uint32_t indexCount, const VkBuffer transformBuffer, uint32_t transformOffset)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo;
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.pNext = nullptr;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VHCHECKRESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		// Transform buffer
		VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

		if (vertexBuffer != VK_NULL_HANDLE)
			vertexBufferDeviceAddress.deviceAddress = vhGetBufferDeviceAddress(device, vertexBuffer);
		if (indexBuffer != VK_NULL_HANDLE)
			indexBufferDeviceAddress.deviceAddress = vhGetBufferDeviceAddress(device, indexBuffer);
		if (transformBuffer != VK_NULL_HANDLE)
			transformBufferDeviceAddress.deviceAddress = vhGetBufferDeviceAddress(device, transformBuffer);

		// Build
		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		accelerationStructureGeometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
		accelerationStructureGeometry.geometry.triangles.maxVertex = vertexCount;
		accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(vh::vhVertex);
		accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		accelerationStructureGeometry.geometry.triangles.indexData = indexBufferDeviceAddress;
		accelerationStructureGeometry.geometry.triangles.transformData = transformBufferDeviceAddress;

		// Get size info
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		const uint32_t numTriangles = indexCount / 3;
		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo, &numTriangles,
			&accelerationStructureBuildSizesInfo);

		vhCreateAccelerationStructureKHR(vmaAllocator, blas, accelerationStructureBuildSizesInfo);
		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = blas.resultBuffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &blas.handleKHR);

		// Create a small scratch buffer used during build of the bottom level acceleration structure
		vhRayTracingScratchBuffer scratchBuffer = vhCreateScratchBuffer(device, vmaAllocator,
			accelerationStructureBuildSizesInfo.buildScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = blas.handleKHR;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = transformOffset;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationBuildStructureRangeInfos = {
			&accelerationStructureBuildRangeInfo };

		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());

		VkMemoryBarrier memoryBarrier;
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.pNext = nullptr;
		memoryBarrier.srcAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		memoryBarrier.dstAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier,
			0, nullptr, 0, nullptr);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = blas.handleKHR;

		blas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);
		blas.geometry = accelerationStructureGeometry;
		blas.rangeInfo = accelerationStructureBuildRangeInfo;
		vhDeleteScratchBuffer(vmaAllocator, scratchBuffer);

		return vh::vhCmdEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer,
			VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	VkResult
		vhUpdateBottomLevelAccelerationStructureKHR(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, vhAccelerationStructure &blas, const VkBuffer vertexBuffer, uint32_t vertexCount, const VkBuffer indexBuffer, uint32_t indexCount, const VkBuffer transformBuffer, uint32_t transformOffset)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo;
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.pNext = nullptr;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VHCHECKRESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		// Transform buffer
		VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
		VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

		if (vertexBuffer != VK_NULL_HANDLE)
			vertexBufferDeviceAddress.deviceAddress = vhGetBufferDeviceAddress(device, vertexBuffer);
		if (indexBuffer != VK_NULL_HANDLE)
			indexBufferDeviceAddress.deviceAddress = vhGetBufferDeviceAddress(device, indexBuffer);
		if (transformBuffer != VK_NULL_HANDLE)
			transformBufferDeviceAddress.deviceAddress = vhGetBufferDeviceAddress(device, transformBuffer);

		// Build
		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		accelerationStructureGeometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
		accelerationStructureGeometry.geometry.triangles.maxVertex = vertexCount;
		accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(vh::vhVertex);
		accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		accelerationStructureGeometry.geometry.triangles.indexData = indexBufferDeviceAddress;
		accelerationStructureGeometry.geometry.triangles.transformData = transformBufferDeviceAddress;

		// Get size info
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		const uint32_t numTriangles = indexCount / 3;
		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo, &numTriangles,
			&accelerationStructureBuildSizesInfo);

		// Create a small scratch buffer used during build of the bottom level acceleration structure
		vhRayTracingScratchBuffer scratchBuffer = vhCreateScratchBuffer(device, vmaAllocator,
			accelerationStructureBuildSizesInfo.updateScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
		accelerationBuildGeometryInfo.srcAccelerationStructure = blas.handleKHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = blas.handleKHR;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &blas.geometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationBuildStructureRangeInfos = {
			&blas.rangeInfo };
		vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());

		VkMemoryBarrier memoryBarrier;
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.pNext = nullptr;
		memoryBarrier.srcAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		memoryBarrier.dstAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier,
			0, nullptr, 0, nullptr);

		vhDeleteScratchBuffer(vmaAllocator, scratchBuffer);

		return vh::vhCmdEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer,
			VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	//--------------------------------------------------------------------------------------------------
	// Create the main acceleration structure that holds all instances of the scene.
	// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
	// the instances, computing the memory requirements for the AS, and building the
	// AS itself #VKRay
	VkResult
		vhCreateTopLevelAccelerationStructureKHR(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, std::vector<vhAccelerationStructure> &blas, vhAccelerationStructure &tlas)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo;
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.pNext = nullptr;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VHCHECKRESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkTransformMatrixKHR transformMatrix = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f };

		std::vector<VkAccelerationStructureInstanceKHR> asInstances;
		asInstances.reserve(blas.size());
		for (int i = 0; i < blas.size(); i++)
		{
			VkAccelerationStructureInstanceKHR instance{};
			instance.transform = transformMatrix;
			instance.instanceCustomIndex = i;
			instance.mask = 0xFF;
			instance.instanceShaderBindingTableRecordOffset = 0;
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			instance.accelerationStructureReference = blas[i].deviceAddress;
			asInstances.push_back(instance);
		}

		VkDeviceSize bufferSize = sizeof(asInstances[0]) * asInstances.size();
		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation;
		vh::vhBufCreateBuffer(vmaAllocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY,
			&stagingBuffer, &stagingBufferAllocation);
		void *data;
		vmaMapMemory(vmaAllocator, stagingBufferAllocation, &data);
		memcpy(data, asInstances.data(), (size_t)bufferSize);
		vmaUnmapMemory(vmaAllocator, stagingBufferAllocation);
		vh::vhBufCreateBuffer(vmaAllocator, bufferSize,
			VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY, &tlas.instancesBuffer, &tlas.instancesBufferAllocation);
		vh::vhBufCopyBuffer(device, graphicsQueue, commandPool, stagingBuffer, tlas.instancesBuffer, bufferSize);
		vmaDestroyBuffer(vmaAllocator, stagingBuffer, stagingBufferAllocation);

		VkDeviceOrHostAddressConstKHR instancesAddress{};
		instancesAddress.deviceAddress = vhGetBufferDeviceAddress(device, tlas.instancesBuffer);
		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		accelerationStructureGeometry.geometry.instances.data = instancesAddress;

		// Get size info
		/*
			The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored.
			Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member
			of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
			*/
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		uint32_t primitive_count = blas.size();

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(
			device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitive_count,
			&accelerationStructureBuildSizesInfo);

		vhCreateAccelerationStructureKHR(vmaAllocator, tlas, accelerationStructureBuildSizesInfo);

		VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
		accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
		accelerationStructureCreateInfo.buffer = tlas.resultBuffer;
		accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
		accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		vkCreateAccelerationStructureKHR(device, &accelerationStructureCreateInfo, nullptr, &tlas.handleKHR);

		// Create a small scratch buffer used during build of the top level acceleration structure
		vhRayTracingScratchBuffer scratchBuffer = vhCreateScratchBuffer(device, vmaAllocator,
			accelerationStructureBuildSizesInfo.buildScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = tlas.handleKHR;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationBuildStructureRangeInfos = {
			&accelerationStructureBuildRangeInfo };

		vkCmdBuildAccelerationStructuresKHR(commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());

		VkMemoryBarrier memoryBarrier;
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.pNext = nullptr;
		memoryBarrier.srcAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		memoryBarrier.dstAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier,
			0, nullptr, 0, nullptr);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = tlas.handleKHR;

		tlas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

		vhDeleteScratchBuffer(vmaAllocator, scratchBuffer);

		return vh::vhCmdEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer,
			VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	VkResult
		vhUpdateTopLevelAccelerationStructureKHR(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, std::vector<vhAccelerationStructure> &blas, vhAccelerationStructure &tlas)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo;
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.pNext = nullptr;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VHCHECKRESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkDeviceOrHostAddressConstKHR instancesAddress{};
		instancesAddress.deviceAddress = vhGetBufferDeviceAddress(device, tlas.instancesBuffer);
		VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
		accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
		accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
		accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
		accelerationStructureGeometry.geometry.instances.data = instancesAddress;

		// Get size info
		/*
			The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored.
			Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member
			of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
			*/
		VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
		accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		accelerationStructureBuildGeometryInfo.geometryCount = 1;
		accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

		uint32_t primitive_count = blas.size();

		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
		accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
		vkGetAccelerationStructureBuildSizesKHR(device,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&accelerationStructureBuildGeometryInfo,
			&primitive_count,
			&accelerationStructureBuildSizesInfo);

		// Create a small scratch buffer used during build of the top level acceleration structure
		vhRayTracingScratchBuffer scratchBuffer = vhCreateScratchBuffer(device, vmaAllocator,
			accelerationStructureBuildSizesInfo.updateScratchSize);

		VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
		accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
		accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
		accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
			VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
		accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
		accelerationBuildGeometryInfo.srcAccelerationStructure = tlas.handleKHR;
		accelerationBuildGeometryInfo.dstAccelerationStructure = tlas.handleKHR;
		accelerationBuildGeometryInfo.geometryCount = 1;
		accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
		accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

		VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
		accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
		accelerationStructureBuildRangeInfo.primitiveOffset = 0;
		accelerationStructureBuildRangeInfo.firstVertex = 0;
		accelerationStructureBuildRangeInfo.transformOffset = 0;
		std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationBuildStructureRangeInfos = {
			&accelerationStructureBuildRangeInfo };

		vkCmdBuildAccelerationStructuresKHR(commandBuffer,
			1,
			&accelerationBuildGeometryInfo,
			accelerationBuildStructureRangeInfos.data());

		VkMemoryBarrier memoryBarrier;
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.pNext = nullptr;
		memoryBarrier.srcAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		memoryBarrier.dstAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &memoryBarrier,
			0, nullptr, 0, nullptr);

		VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
		accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
		accelerationDeviceAddressInfo.accelerationStructure = tlas.handleKHR;
		tlas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &accelerationDeviceAddressInfo);

		vhDeleteScratchBuffer(vmaAllocator, scratchBuffer);

		return vh::vhCmdEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer,
			VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	VkResult
		vhCreateBottomLevelAccelerationStructureNV(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, vhAccelerationStructure &blas, const VkBuffer vertexBuffer, uint32_t vertexCount, const VkBuffer indexBuffer, uint32_t indexCount, const VkBuffer transformBuffer, uint32_t transformOffset)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo;
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.pNext = nullptr;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VHCHECKRESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		blas.blasGenerator.AddVertexBuffer(vertexBuffer, 0, vertexCount, sizeof(vh::vhVertex),
			indexBuffer, 0, indexCount,
			transformBuffer, transformOffset);

		// Once the overall size of the geometry is known, we can create the handle
		// for the acceleration structure
		blas.handleNV = blas.blasGenerator.CreateAccelerationStructure(device, VK_TRUE);

		// The AS build requires some scratch space to store temporary information.
		// The amount of scratch memory is dependent on the scene complexity.
		VkDeviceSize scratchSizeInBytes = 0;
		// The final AS also needs to be stored in addition to the existing vertex
		// buffers. It size is also dependent on the scene complexity.
		VkDeviceSize resultSizeInBytes = 0;
		blas.blasGenerator.ComputeASBufferSizes(device, blas.handleNV, &scratchSizeInBytes, &resultSizeInBytes);

		// Once the sizes are obtained, the application is responsible for allocating
		// the necessary buffers. Since the entire generation will be done on the GPU,
		// we can directly allocate those in device local mem
		nv_helpers_vk::createBuffer(physicalDevice, device, scratchSizeInBytes,
			VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, &blas.scratchBuffer,
			&blas.scratchMem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		nv_helpers_vk::createBuffer(physicalDevice, device, resultSizeInBytes,
			VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, &blas.resultBuffer,
			&blas.resultMem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Build the acceleration structure. Note that this call integrates a barrier
		// on the generated AS, so that it can be used to compute a top-level AS right
		// after this method.
		blas.blasGenerator.Generate(device, commandBuffer, blas.handleNV, blas.scratchBuffer,
			0, blas.resultBuffer, blas.resultMem, false, VK_NULL_HANDLE);

		return vh::vhCmdEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer,
			VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	VkResult
		vhUpdateBottomLevelAccelerationStructureNV(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, vhAccelerationStructure &blas, const VkBuffer vertexBuffer, uint32_t vertexCount, const VkBuffer indexBuffer, uint32_t indexCount, const VkBuffer transformBuffer, uint32_t transformOffset)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo;
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.pNext = nullptr;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VHCHECKRESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		blas.blasGenerator.Generate(device, commandBuffer, blas.handleNV, blas.scratchBuffer,
			0, blas.resultBuffer, blas.resultMem, true, blas.handleNV);

		return vh::vhCmdEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer,
			VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	VkResult
		vhCreateTopLevelAccelerationStructureNV(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, std::vector<vhAccelerationStructure> &blas, vhAccelerationStructure &tlas)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo;
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.pNext = nullptr;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VHCHECKRESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		glm::mat4x4 transformMatrix(1.0);

		// Gather all the instances into the builder helper
		for (size_t i = 0; i < blas.size(); i++)
		{
			// For each instance we set its instance index to its index i in the instance vector, and set
			// its hit group index to 0. The hit group index defines which entry of the shader binding
			// table will contain the hit group to be executed when hitting this instance.
			tlas.tlasGenerator.AddInstance(blas[i].handleNV, transformMatrix, static_cast<uint32_t>(i),
				static_cast<uint32_t>(0));
		}

		// Once all instances have been added, we can create the handle for the TLAS
		tlas.handleNV = tlas.tlasGenerator.CreateAccelerationStructure(device, VK_TRUE);

		// As for the bottom-level AS, the building the AS requires some scratch
		// space to store temporary data in addition to the actual AS. In the case
		// of the top-level AS, the instance descriptors also need to be stored in
		// GPU memory. This call outputs the memory requirements for each (scratch,
		// results, instance descriptors) so that the application can allocate the
		// corresponding memory
		VkDeviceSize scratchSizeInBytes, resultSizeInBytes, instanceDescsSizeInBytes;
		tlas.tlasGenerator.ComputeASBufferSizes(device, tlas.handleNV, &scratchSizeInBytes,
			&resultSizeInBytes, &instanceDescsSizeInBytes);

		// Create the scratch and result buffers. Since the build is all done on
		// GPU, those can be allocated in device local memory
		nv_helpers_vk::createBuffer(physicalDevice, device, scratchSizeInBytes,
			VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, &tlas.scratchBuffer,
			&tlas.scratchMem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		nv_helpers_vk::createBuffer(physicalDevice, device, resultSizeInBytes,
			VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, &tlas.resultBuffer,
			&tlas.resultMem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		// The buffer describing the instances: ID, shader binding information,
		// matrices ... Those will be copied into the buffer by the helper through
		// mapping, so the buffer has to be allocated in host visible memory.

		nv_helpers_vk::createBuffer(physicalDevice, device,
			instanceDescsSizeInBytes, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
			&tlas.instancesBuffer, &tlas.instancesMem,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		tlas.tlasGenerator.Generate(device, commandBuffer, tlas.handleNV,
			tlas.scratchBuffer, 0, tlas.resultBuffer,
			tlas.resultMem, tlas.instancesBuffer,
			tlas.instancesMem, false, VK_NULL_HANDLE);

		return vh::vhCmdEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer,
			VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

	VkResult vhUpdateTopLevelAccelerationStructureNV(VkDevice device, VmaAllocator vmaAllocator, VkCommandPool commandPool, VkQueue graphicsQueue, std::vector<vhAccelerationStructure> &blas, vhAccelerationStructure &tlas)
	{
		VkCommandBufferAllocateInfo commandBufferAllocateInfo;
		commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		commandBufferAllocateInfo.pNext = nullptr;
		commandBufferAllocateInfo.commandPool = commandPool;
		commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		commandBufferAllocateInfo.commandBufferCount = 1;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		VHCHECKRESULT(vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer));

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		tlas.tlasGenerator.Generate(device, commandBuffer, tlas.handleNV,
			tlas.scratchBuffer, 0, tlas.resultBuffer,
			tlas.resultMem, tlas.instancesBuffer,
			tlas.instancesMem, true, tlas.handleNV);

		return vh::vhCmdEndSingleTimeCommands(device, graphicsQueue, commandPool, commandBuffer,
			VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE);
	}

} // namespace vh