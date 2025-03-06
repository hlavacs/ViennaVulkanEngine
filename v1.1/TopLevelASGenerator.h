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



Example:

// Add all instances of the scene
TopLevelAS topLevelAS;
topLevelAS.AddInstance(instances1, matrix1, instanceId1, hitGroupIndex1);
topLevelAS.AddInstance(instances2, matrix2, instanceId2, hitGroupIndex2);
...
structure =
topLevelAS.CreateAccelerationStructure(device, VK_TRUE);

// Find the size of the buffers to store the AS
VkDeviceSize scratchSize, resultSize, instanceDescsSize;
topLevelAS.ComputeASBufferSizes(device, structure, &scratchSize, &resultSize, &instanceDescsSize);

// Create the AS buffers
AccelerationStructureBuffers buffers;
buffers.pScratch = nv_helpers_vk::CreateBuffer(..., scratchSizeInBytes, ...);
buffers.pResult = nv_helpers_vk::CreateBuffer(..., resultSizeInBytes, ...);
buffers.pInstanceDesc = nv_helpers_vk::CreateBuffer(..., resultSizeInBytes, ...);

// Generate the top level acceleration structure
m_topLevelASGenerator.Generate(
device, commandBuffer, structure, buffers.scratchBuffer,
buffers.resultBuffer, buffers.resultMem, buffers.instancesBuffer,
buffers.instancesMem, updateOnly, updateOnly ? structure : VK_NULL_HANDLE);

return buffers;

*/

#pragma once
#define GLM_ENABLE_EXPERIMENTAL

#include "glm/gtx/transform.hpp"

#include "vulkan/vulkan.h"

#include <vector>

namespace nv_helpers_vk
{
	/// Helper class to generate top-level acceleration structures for raytracing
	class TopLevelASGenerator
	{
	public:
		/// Add an instance to the top-level acceleration structure. The instance is
		/// represented by a bottom-level AS, a transform, an instance ID and the
		/// index of the hit group indicating which shaders are executed upon hitting
		/// any geometry within the instance
		void AddInstance(VkAccelerationStructureNV bottomLevelAS, /// Bottom-level acceleration structure containing
						 /// the actual geometric data of the instance
			const glm::mat4x4 &transform, /// Transform matrix to apply to the instance,
			/// allowing the same bottom-level AS to be used
			/// at several world-space positions
			uint32_t instanceID, /// Instance ID, which can be used in the shaders to
			/// identify this specific instance
			uint32_t hitGroupIndex /// Hit group index, corresponding the the index of the
			/// hit group in the Shader Binding Table that will be
			/// invocated upon hitting the geometry
		);

		/// Create the opaque acceleration structure descriptor, which will be used in the estimation of
		/// the AS size and the generation itself. The allowUpdate flag indicates if the AS will need
		/// dynamic refitting. This has to be called after adding all the instances.
		VkAccelerationStructureNV CreateAccelerationStructure(VkDevice device,
			VkBool32 allowUpdate = VK_FALSE);

		/// Compute the size of the scratch space required to build the acceleration
		/// structure, as well as the size of the resulting structure. The allocation
		/// of the buffers is then left to the application
		void ComputeASBufferSizes(VkDevice device, /*/ Device on which the build will be performed */
			VkAccelerationStructureNV accelerationStructure,
			VkDeviceSize *scratchSizeInBytes,
			/*/ Required scratch memory on the GPU to */ /*/ build the acceleration structure */
			VkDeviceSize *resultSizeInBytes,
			/*/ Required GPU memory to store the */ /*/ acceleration structure */
			VkDeviceSize *instancesSizeInBytes /*/ Required GPU memory to store instance */
			/*/ descriptors, containing the matrices, */ /*/ indices etc. */
		);

		/// Enqueue the construction of the acceleration structure on a command list,
		/// using application-provided buffers and possibly a pointer to the previous
		/// acceleration structure in case of iterative updates. Note that the update
		/// can be done in place: the result and previousResult pointers can be the
		/// same.
		void Generate(VkDevice device,
			VkCommandBuffer commandBuffer, /// Command list on which the build will be enqueued
			VkAccelerationStructureNV accelerationStructure,
			VkBuffer scratchBuffer, /// Scratch buffer used by the builder to
			/// store temporary data
			VkDeviceSize scratchOffset, /// Offset in the scratch buffer at which the builder can
			/// start writing memory
			VkBuffer resultBuffer, /// Result buffer storing the acceleration structure
			VkDeviceMemory resultMem,
			VkBuffer instancesBuffer, /// Auxiliary result buffer containing the instance
			/// descriptors, has to be in upload heap
			VkDeviceMemory instancesMem,
			VkBool32 updateOnly = false, /// If true, simply refit the
			/// existing acceleration structure
			VkAccelerationStructureNV previousResult = nullptr /// Optional previous acceleration
			/// structure, used if an
			/// iterative update is requested
		);

	private:
		/// Geometry instance, with the layout expected by VK_NV_ray_tracing
		struct VkGeometryInstance
		{
			/// Transform matrix, containing only the top 3 rows
			float transform[12];
			/// Instance index
			uint32_t instanceId : 24;
			/// Visibility mask
			uint32_t mask : 8;
			/// Index of the hit group which will be invoked when a ray hits the instance
			uint32_t instanceOffset : 24;
			/// Instance flags, such as culling
			uint32_t flags : 8;
			/// Opaque handle of the bottom-level acceleration structure
			uint64_t accelerationStructureHandle;
		};

		static_assert(sizeof(VkGeometryInstance) == 64,
			"VkGeometryInstance structure compiles to incorrect size");

		/// Helper struct storing the instance data
		struct Instance
		{
			Instance(VkAccelerationStructureNV blAS, const glm::mat4x4 &tr, uint32_t iID, uint32_t hgId);

			/// Bottom-level AS
			VkAccelerationStructureNV bottomLevelAS;
			/// Transform matrix
			const glm::mat4x4 transform;
			/// Instance ID visible in the shader
			uint32_t instanceID;
			/// Hit group index used to fetch the shaders from the SBT
			uint32_t hitGroupIndex;
		};

		/// Construction flags, indicating whether the AS supports iterative updates
		VkBuildAccelerationStructureFlagsNV m_flags;
		/// Instances contained in the top-level AS
		std::vector<Instance> m_instances;

		/// Size of the temporary memory used by the TLAS builder
		VkDeviceSize m_scratchSizeInBytes;
		/// Size of the buffer containing the instance descriptors
		VkDeviceSize m_instanceDescsSizeInBytes;
		/// Size of the buffer containing the TLAS
		VkDeviceSize m_resultSizeInBytes;
	};
} // namespace nv_helpers_vk
