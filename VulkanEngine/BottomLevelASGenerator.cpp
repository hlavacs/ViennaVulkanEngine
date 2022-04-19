/*-----------------------------------------------------------------------
Copyright (c) 2014-2018, NVIDIA. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Neither the name of its contributors may be used to endorse
or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------*/

/*
Contacts for feedback:
- pgautron@nvidia.com (Pascal Gautron)
- mlefrancois@nvidia.com (Martin-Karl Lefrancois)
*/

#include "VHHelper.h"
#include "VKHelpers.h"

#include "BottomLevelASGenerator.h"

namespace nv_helpers_vk
{
	//--------------------------------------------------------------------------------------------------
	// Add a vertex buffer in GPU memory into the acceleration structure. The
	// vertices are supposed to be represented by 3 float32 value
	void BottomLevelASGenerator::AddVertexBuffer(
		VkBuffer vertexBuffer,
		VkDeviceSize vertexOffsetInBytes,
		uint32_t vertexCount,
		VkDeviceSize vertexSizeInBytes,
		VkBuffer transformBuffer,
		VkDeviceSize transformOffsetInBytes,
		bool isOpaque /* = true */
	)
	{
		AddVertexBuffer(vertexBuffer, vertexOffsetInBytes, vertexCount, vertexSizeInBytes,
			nullptr, 0, 0,
			transformBuffer, transformOffsetInBytes, isOpaque);
	}

	//--------------------------------------------------------------------------------------------------
	// Add a vertex buffer along with its index buffer in GPU memory into the
	// acceleration structure. The vertices are supposed to be represented by 3
	// float32 value. This implementation limits the original flexibility of the
	// API:
	//   - triangles (no custom intersector support)
	//   - 3xfloat32 format
	//   - 32-bit indices
	void BottomLevelASGenerator::AddVertexBuffer(
		VkBuffer vertexBuffer,
		VkDeviceSize vertexOffsetInBytes,
		uint32_t vertexCount,
		VkDeviceSize vertexSizeInBytes,
		VkBuffer indexBuffer,
		VkDeviceSize indexOffsetInBytes,
		uint32_t indexCount,
		VkBuffer transformBuffer,
		VkDeviceSize transformOffsetInBytes,
		bool isOpaque)
	{
		VkGeometryNV geometry;
		geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
		geometry.pNext = nullptr;
		geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
		geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
		geometry.geometry.triangles.pNext = nullptr;
		geometry.geometry.triangles.vertexData = vertexBuffer;
		geometry.geometry.triangles.vertexOffset = vertexOffsetInBytes;
		geometry.geometry.triangles.vertexCount = vertexCount;
		geometry.geometry.triangles.vertexStride = vertexSizeInBytes;
		// Limitation to 3xfloat32 for vertices
		geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		geometry.geometry.triangles.indexData = indexBuffer;
		geometry.geometry.triangles.indexOffset = indexOffsetInBytes;
		geometry.geometry.triangles.indexCount = indexCount;
		// Limitation to 32-bit indices
		geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		geometry.geometry.triangles.transformData = transformBuffer;
		geometry.geometry.triangles.transformOffset = transformOffsetInBytes;
		geometry.geometry.aabbs = { VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV };
		geometry.flags = isOpaque ? VK_GEOMETRY_OPAQUE_BIT_NV : 0;

		m_vertexBuffers.push_back(geometry);
	}

	//--------------------------------------------------------------------------------------------------
	// Create the opaque acceleration structure descriptor, which will be used in the estimation of
	// the AS size and the generation itself. The allowUpdate flag indicates if the AS will need
	// dynamic refitting. This has to be called after adding all the geometry.
	VkAccelerationStructureNV BottomLevelASGenerator::CreateAccelerationStructure(VkDevice device,
		VkBool32 allowUpdate)
	{
		// The generated AS can support iterative updates. This may change the final
		// size of the AS as well as the temporary memory requirements, and hence has
		// to be set before the actual build
		m_flags = allowUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV : 0;

		// Create the descriptor of the acceleration structure, which contains the number of geometry
		// descriptors it will contain
		VkAccelerationStructureInfoNV accelerationStructureInfo{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV };
		accelerationStructureInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
		accelerationStructureInfo.flags = m_flags;
		accelerationStructureInfo.instanceCount =
			0; // The bottom-level AS can only contain explicit geometry, and no instances
		accelerationStructureInfo.geometryCount = static_cast<uint32_t>(m_vertexBuffers.size());
		accelerationStructureInfo.pGeometries = m_vertexBuffers.data();

		VkAccelerationStructureCreateInfoNV accelerationStructureCreateInfo{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV };
		accelerationStructureCreateInfo.pNext = nullptr;
		accelerationStructureCreateInfo.info = accelerationStructureInfo;
		accelerationStructureCreateInfo.compactedSize = 0;

		VkAccelerationStructureNV accelerationStructure;
		VkResult code = vkCreateAccelerationStructureNV(device, &accelerationStructureCreateInfo, nullptr,
			&accelerationStructure);
		if (code != VK_SUCCESS)
		{
			throw std::logic_error("vkCreateAccelerationStructureNV failed");
		}

		return accelerationStructure;
	}

	//--------------------------------------------------------------------------------------------------
	// Compute the size of the scratch space required to build the acceleration
	// structure, as well as the size of the resulting structure. The allocation of
	// the buffers is then left to the application
	void BottomLevelASGenerator::ComputeASBufferSizes(
		VkDevice device,
		VkAccelerationStructureNV accelerationStructure,
		VkDeviceSize *scratchSizeInBytes,
		VkDeviceSize *resultSizeInBytes)
	{
		// Create a descriptor for the memory requirements, and provide the acceleration structure
		// descriptor
		VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
		memoryRequirementsInfo.sType =
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
		memoryRequirementsInfo.pNext = nullptr;
		memoryRequirementsInfo.accelerationStructure = accelerationStructure;
		memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;

		// This descriptor already contains the geometry info, so we can directly compute the estimated AS
		// size and required scratch memory
		VkMemoryRequirements2 memoryRequirements;
		vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo,
			&memoryRequirements);

		// Size of the resulting AS
		m_resultSizeInBytes = memoryRequirements.memoryRequirements.size;

		// Store the memory requirements for use during build/update
		memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV;
		vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo,
			&memoryRequirements);
		m_scratchSizeInBytes = memoryRequirements.memoryRequirements.size;

		memoryRequirementsInfo.type =
			VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_NV;
		vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo,
			&memoryRequirements);

		m_scratchSizeInBytes = m_scratchSizeInBytes > memoryRequirements.memoryRequirements.size ?
			m_scratchSizeInBytes :
			memoryRequirements.memoryRequirements.size;

		*resultSizeInBytes = m_resultSizeInBytes;
		*scratchSizeInBytes = m_scratchSizeInBytes;
	}

	//--------------------------------------------------------------------------------------------------
	// Enqueue the construction of the acceleration structure on a command list, using
	// application-provided buffers and possibly a pointer to the previous acceleration structure in
	// case of iterative updates. Note that the update can be done in place: the result and
	// previousResult pointers can be the same.
	void BottomLevelASGenerator::Generate(
		VkDevice device,
		VkCommandBuffer commandList,
		VkAccelerationStructureNV accelerationStructure,
		VkBuffer scratchBuffer,
		VkDeviceSize scratchOffset,
		VkBuffer resultBuffer,
		VkDeviceMemory resultMem,
		VkBool32 updateOnly,
		VkAccelerationStructureNV previousResult)
	{
		// Sanity checks
		if (m_flags != VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV && updateOnly)
		{
			throw std::logic_error("Cannot update a bottom-level AS not originally built for updates");
		}
		if (updateOnly && previousResult == VK_NULL_HANDLE)
		{
			throw std::logic_error("Bottom-level hierarchy update requires the previous hierarchy");
		}

		if (m_resultSizeInBytes == 0 || m_scratchSizeInBytes == 0)
		{
			throw std::logic_error(
				"Invalid scratch and result buffer sizes - ComputeASBufferSizes needs "
				"to be called before Build");
		}

		// Bind the acceleration structure descriptor to the actual memory that will contain it
		VkBindAccelerationStructureMemoryInfoNV bindInfo;
		bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
		bindInfo.pNext = nullptr;
		bindInfo.accelerationStructure = accelerationStructure;
		bindInfo.memory = resultMem;
		bindInfo.memoryOffset = 0;
		bindInfo.deviceIndexCount = 0;
		bindInfo.pDeviceIndices = nullptr;

		VkResult code = vkBindAccelerationStructureMemoryNV(device, 1, &bindInfo);

		if (code != VK_SUCCESS)
		{
			throw std::logic_error("vkBindAccelerationStructureMemoryNV failed");
		}

		// Build the actual bottom-level acceleration structure
		VkAccelerationStructureInfoNV buildInfo;
		buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
		buildInfo.pNext = nullptr;
		buildInfo.flags = m_flags;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
		buildInfo.geometryCount = static_cast<uint32_t>(m_vertexBuffers.size());
		buildInfo.pGeometries = m_vertexBuffers.data();
		buildInfo.instanceCount = 0;

		vkCmdBuildAccelerationStructureNV(commandList, &buildInfo, VK_NULL_HANDLE, 0, updateOnly,
			accelerationStructure,
			updateOnly ? previousResult : VK_NULL_HANDLE, scratchBuffer,
			scratchOffset);

		// Wait for the builder to complete by setting a barrier on the resulting buffer. This is
		// particularly important as the construction of the top-level hierarchy may be called right
		// afterwards, before executing the command list.
		VkMemoryBarrier memoryBarrier;
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.pNext = nullptr;
		memoryBarrier.srcAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
		memoryBarrier.dstAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;

		vkCmdPipelineBarrier(commandList, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier,
			0, nullptr, 0, nullptr);
	}

} // namespace nv_helpers_vk
