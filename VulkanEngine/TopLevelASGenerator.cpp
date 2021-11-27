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

The top-level hierarchy is used to store a set of instances represented by
bottom-level hierarchies in a way suitable for fast intersection at runtime. To
be built, this data structure requires some scratch space which has to be
allocated by the application. Similarly, the resulting data structure is stored
in an application-controlled buffer.

To be used, the application must first add all the instances to be contained in
the final structure, using AddInstance. After all instances have been added,
ComputeASBufferSizes will prepare the build, and provide the required sizes for
the scratch data and the final result. The Build call will finally compute the
acceleration structure and store it in the result buffer.

Note that the build is enqueued in the command list, meaning that the scratch
buffer needs to be kept until the command list execution is finished.

*/

#include "VHHelper.h"
#include "VKHelpers.h"

#include "TopLevelASGenerator.h"

namespace nv_helpers_vk
{
	//--------------------------------------------------------------------------------------------------
	//
	// Add an instance to the top-level acceleration structure. The instance is
	// represented by a bottom-level AS, a transform, an instance ID and the index
	// of the hit group indicating which shaders are executed upon hitting any
	// geometry within the instance
	void TopLevelASGenerator::AddInstance(VkAccelerationStructureNV bottomLevelAS, // Bottom-level acceleration structure containing the
										  // actual geometric data of the instance
		const glm::mat4x4 &transform, // Transform matrix to apply to the instance, allowing the
		// same bottom-level AS to be used at several world-space
		// positions
		uint32_t instanceID, // Instance ID, which can be used in the shaders to
		// identify this specific instance
		uint32_t hitGroupIndex // Hit group index, corresponding the the index of the
		// hit group in the Shader Binding Table that will be
		// invocated upon hitting the geometry
	)
	{
		m_instances.emplace_back(Instance(bottomLevelAS, transform, instanceID, hitGroupIndex));
	}

	//--------------------------------------------------------------------------------------------------
	//
	// Create the opaque acceleration structure descriptor, which will be used in the estimation of
	// the AS size and the generation itself. The allowUpdate flag indicates if the AS will need
	// dynamic refitting. This has to be called after adding all the instances.
	VkAccelerationStructureNV TopLevelASGenerator::CreateAccelerationStructure(VkDevice device,
		VkBool32 allowUpdate /* = VK_FALSE */)
	{
		// The generated AS can support iterative updates. This may change the final
		// size of the AS as well as the temporary memory requirements, and hence has
		// to be set before the actual build
		m_flags = allowUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_NV : 0;

		VkAccelerationStructureInfoNV info{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV };
		info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
		info.flags = m_flags;
		info.instanceCount = static_cast<uint32_t>(
			m_instances.size()); // The descriptor already contains the number of instances
		info.geometryCount = 0; // Since this is a top-level AS, it does not contain any geometry
		info.pGeometries = VK_NULL_HANDLE;

		VkAccelerationStructureCreateInfoNV accelerationStructureInfo{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV };
		accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
		accelerationStructureInfo.info = info;
		accelerationStructureInfo.pNext = nullptr;
		accelerationStructureInfo.compactedSize = 0;

		VkAccelerationStructureNV accelerationStructure;
		VkResult code = vkCreateAccelerationStructureNV(device, &accelerationStructureInfo, nullptr,
			&accelerationStructure);

		if (code != VK_SUCCESS)
		{
			throw std::logic_error("vkCreateAccelerationStructureNV failed");
		}

		return accelerationStructure;
	}

	//--------------------------------------------------------------------------------------------------
	//
	// Compute the size of the scratch space required to build the acceleration
	// structure, as well as the size of the resulting structure. The allocation of
	// the buffers is then left to the application
	void TopLevelASGenerator::ComputeASBufferSizes(VkDevice device, /* Device on which the build will be performed */
		VkAccelerationStructureNV accelerationStructure,
		VkDeviceSize *scratchSizeInBytes,
		/* Required scratch memory on the GPU to build the acceleration structure */
		VkDeviceSize *resultSizeInBytes,
		/* Required GPU memory to store the acceleration structure */
		VkDeviceSize *instancesSizeInBytes /* Required GPU memory to store instance */
		/* descriptors, containing the matrices, indices etc. */
	)
	{
		// Create a descriptor indicating which memory requirements we want to obtain
		VkAccelerationStructureMemoryRequirementsInfoNV memoryRequirementsInfo;
		memoryRequirementsInfo.sType =
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
		memoryRequirementsInfo.pNext = nullptr;
		memoryRequirementsInfo.type = VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV;
		memoryRequirementsInfo.accelerationStructure = accelerationStructure;

		// Query the memory requirements. Note that the number of instances in the AS has already
		// been provided when creating the AS descriptor
		VkMemoryRequirements2 memoryRequirements;
		vkGetAccelerationStructureMemoryRequirementsNV(device, &memoryRequirementsInfo,
			&memoryRequirements);

		// Size of the resulting acceleration structure
		m_resultSizeInBytes = memoryRequirements.memoryRequirements.size;

		// Store the memory requirements for use during build
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

		// Amount of memory required to store the instance descriptors
		m_instanceDescsSizeInBytes = m_instances.size() * sizeof(VkGeometryInstance);
		*instancesSizeInBytes = m_instanceDescsSizeInBytes;
	}

	//--------------------------------------------------------------------------------------------------
	//
	// Enqueue the construction of the acceleration structure on a command list,
	// using application-provided buffers and possibly a pointer to the previous
	// acceleration structure in case of iterative updates. Note that the update can
	// be done in place: the result and previousResult descriptors can be the same.
	void TopLevelASGenerator::Generate(VkDevice device, // Device on which the generation will be performed
		VkCommandBuffer commandBuffer, // Command list on which the build will be enqueued
		VkAccelerationStructureNV accelerationStructure,
		VkBuffer scratchBuffer, // Scratch buffer used by the builder to
		// store temporary data
		VkDeviceSize
		scratchOffset, // Offset in the scratch buffer at which the builder can start writing memory
		VkBuffer resultBuffer, // Result buffer storing the acceleration structure
		VkDeviceMemory resultMem,
		VkBuffer instancesBuffer, // Auxiliary result buffer containing the instance
		// descriptors, has to be in upload heap
		VkDeviceMemory instancesMem,
		VkBool32 updateOnly /*= false*/, // If true, simply refit the
		// existing acceleration structure
		VkAccelerationStructureNV previousResult /*= nullptr*/ // Optional previous acceleration
		// structure, used if an iterative
		// update is requested
	)
	{
		// For each instance, build the corresponding instance descriptor
		std::vector<VkGeometryInstance> geometryInstances;
		for (const auto &inst : m_instances)
		{
			if (!inst.bottomLevelAS)
				continue;
			uint64_t accelerationStructureHandle = 0;
			VkResult code = vkGetAccelerationStructureHandleNV(device, inst.bottomLevelAS, sizeof(uint64_t),
				&accelerationStructureHandle);
			if (code != VK_SUCCESS)
			{
				throw std::logic_error("vkGetAccelerationStructureHandleNV failed");
			}

			VkGeometryInstance gInst;
			glm::mat4x4 transp = glm::transpose(inst.transform);
			memcpy(gInst.transform, &transp, sizeof(gInst.transform));
			gInst.instanceId = inst.instanceID;
			// The visibility mask is always set of 0xFF, but if some instances would need to be ignored in
			// some cases, this flag should be passed by the application
			gInst.mask = 0xff;
			// Set the hit group index, that will be used to find the shader code to execute when hitting
			// the geometry
			gInst.instanceOffset = inst.hitGroupIndex;
			// Disable culling - more fine control could be provided by the application
			gInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
			gInst.accelerationStructureHandle = accelerationStructureHandle;
			geometryInstances.push_back(gInst);
		}

		// Copy the instance descriptors into the provided mappable buffer
		VkDeviceSize instancesBufferSize = geometryInstances.size() * sizeof(VkGeometryInstance);
		void *data;
		vkMapMemory(device, instancesMem, 0, instancesBufferSize, 0, &data);
		memcpy(data, geometryInstances.data(), instancesBufferSize);
		vkUnmapMemory(device, instancesMem);

		// Bind the acceleration structure descriptor to the actual memory that will store the AS itself
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

		// Build the acceleration structure and store it in the result memory
		VkAccelerationStructureInfoNV buildInfo;
		buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
		buildInfo.pNext = nullptr;
		buildInfo.flags = m_flags;
		buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
		buildInfo.instanceCount = static_cast<uint32_t>(geometryInstances.size());
		buildInfo.geometryCount = 0;
		buildInfo.pGeometries = nullptr;

		vkCmdBuildAccelerationStructureNV(commandBuffer, &buildInfo, instancesBuffer, 0, updateOnly,
			accelerationStructure,
			updateOnly ? previousResult : VK_NULL_HANDLE, scratchBuffer,
			scratchOffset);

		// Ensure that the build will be finished before using the AS using a barrier
		VkMemoryBarrier memoryBarrier;
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.pNext = nullptr;
		memoryBarrier.srcAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
		memoryBarrier.dstAccessMask =
			VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memoryBarrier,
			0, nullptr, 0, nullptr);
	}

	//--------------------------------------------------------------------------------------------------
	//
	//
	TopLevelASGenerator::Instance::Instance(VkAccelerationStructureNV blAS,
		const glm::mat4x4 &tr,
		uint32_t iID,
		uint32_t hgId)
		: bottomLevelAS(blAS),
		transform(tr),
		instanceID(iID),
		hitGroupIndex(hgId)
	{
	}

} // namespace nv_helpers_vk
