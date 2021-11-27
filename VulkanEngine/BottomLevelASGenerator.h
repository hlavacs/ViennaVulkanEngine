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

The bottom-level hierarchy is used to store the triangle data in a way suitable
for fast ray-triangle intersection at runtime. To be built, this data structure
requires some scratch space which has to be allocated by the application.
Similarly, the resulting data structure is stored in an application-controlled
buffer.

To be used, the application must first add all the vertex buffers to be
contained in the final structure, using AddVertexBuffer. After all buffers have
been added, ComputeASBufferSizes will prepare the build, and provide the
required sizes for the scratch data and the final result. The Build call will
finally compute the acceleration structure and store it in the result buffer.

Note that the build is enqueued in the command list, meaning that the scratch
buffer needs to be kept until the command list execution is finished.


Example:

// Add the vertex buffers (geometry)
BottomLevelAS bottomLevelAS;
bottomLevelAS.AddVertexBuffer(vertexBuffer1, 0, vertexCount1, sizeof(Vertex), transformBuffer1, 0);
bottomLevelAS.AddVertexBuffer(vertexBuffer2, 0, vertexCount2, sizeof(Vertex), transformBuffer2, 0);
...

VkAccelerationStructureNV structure = bottomLevelAS.CreateAccelerationStructure(VkCtx.getDevice(),
false);

// Find the size for the buffers
UINT64 scratchSizeInBytes = 0;
UINT64 resultSizeInBytes = 0;
bottomLevelAS.ComputeASBufferSizes(device, structure, &scratchSizeInBytes, &resultSizeInBytes);
AccelerationStructureBuffers buffers;
buffers.scratchBuffer = nv_helpers_vk::CreateBuffer(..., scratchSizeInBytes, ...);
buffers.resultBuffer = nv_helpers_vk::CreateBuffer(..., resultSizeInBytes, ...);

// Generate acceleration structure
bottomLevelAS.Generate(device, commandBuffer, structure, buffers.scratchBuffer,
buffers.resultBuffer, false, nullptr);

return buffers;

*/

#pragma once

#include "vulkan/vulkan.h"

#include <stdexcept>
#include <vector>

namespace nv_helpers_vk
{
	/// Helper class to generate bottom-level acceleration structures for raytracing
	class BottomLevelASGenerator
	{
	public:
		/// Add a vertex buffer in GPU memory into the acceleration structure. The
		/// vertices are supposed to be represented by 3 float32 value. Indices are
		/// implicit.
		void AddVertexBuffer(VkBuffer vertexBuffer, /// Buffer containing the vertex coordinates,
							 /// possibly interleaved with other vertex data
			VkDeviceSize vertexOffsetInBytes, /// Offset of the first vertex in the
			/// vertex buffer
			uint32_t vertexCount, /// Number of vertices to consider
			/// in the buffer
			VkDeviceSize vertexSizeInBytes, /// Size of a vertex including all
			/// its other data, used to stride
			/// in the buffer
			VkBuffer transformBuffer, /// Buffer containing a 4x4 transform
			/// matrix in GPU memory, to be applied
			/// to the vertices. This buffer cannot
			/// be nullptr
			VkDeviceSize transformOffsetInBytes, /// Offset of the transform matrix in
			/// the transform buffer
			bool isOpaque = true /// If true, the geometry is considered opaque,
			/// optimizing the search for a closest hit
		);

		/// Add a vertex buffer along with its index buffer in GPU memory into the acceleration structure.
		/// The vertices are supposed to be represented by 3 float32 value, and the indices are 32-bit
		/// unsigned ints
		void AddVertexBuffer(VkBuffer vertexBuffer, /// Buffer containing the vertex coordinates,
							 /// possibly interleaved with other vertex data
			VkDeviceSize vertexOffsetInBytes, /// Offset of the first vertex in the
			/// vertex buffer
			uint32_t vertexCount, /// Number of vertices to consider
			/// in the buffer
			VkDeviceSize vertexSizeInBytes, /// Size of a vertex including
			/// all its other data,
			/// used to stride in the buffer
			VkBuffer indexBuffer, /// Buffer containing the vertex indices
			/// describing the triangles
			VkDeviceSize indexOffsetInBytes, /// Offset of the first index in
			/// the index buffer
			uint32_t indexCount, /// Number of indices to consider in the buffer
			VkBuffer transformBuffer, /// Buffer containing a 4x4 transform
			/// matrix in GPU memory, to be applied
			/// to the vertices. This buffer cannot
			/// be nullptr
			VkDeviceSize transformOffsetInBytes, /// Offset of the transform matrix in
			/// the transform buffer
			bool isOpaque = true /// If true, the geometry is considered opaque,
			/// optimizing the search for a closest hit
		);

		/// Create the opaque acceleration structure descriptor, which will be used in the estimation of
		/// the AS size and the generation itself. The allowUpdate flag indicates if the AS will need
		/// dynamic refitting. This has to be called after adding all the geometry.
		VkAccelerationStructureNV CreateAccelerationStructure(VkDevice device,
			VkBool32 allowUpdate = VK_FALSE);

		/// Compute the size of the scratch space required to build the acceleration structure, as well as
		/// the size of the resulting structure. The allocation of the buffers is then left to the
		/// application
		void ComputeASBufferSizes(
			VkDevice device, /// Device on which the build will be performed
			VkAccelerationStructureNV accelerationStructure,
			VkDeviceSize *scratchSizeInBytes, /// Required scratch memory on the GPU to
			/// build the acceleration structure
			VkDeviceSize *resultSizeInBytes /// Required GPU memory to store the
			/// acceleration structure
		);

		/// Enqueue the construction of the acceleration structure on a command list, using
		/// application-provided buffers and possibly a pointer to the previous acceleration structure in
		/// case of iterative updates. Note that the update can be done in place: the result and
		/// previousResult pointers can be the same.
		void Generate(VkDevice device,
			VkCommandBuffer commandList, /// Command list on which the build will be enqueued
			VkAccelerationStructureNV accelerationStructure,
			VkBuffer scratchBuffer, /// Scratch buffer used by the builder to
			/// store temporary data
			VkDeviceSize scratchOffset, /// Offset in the scratch buffer at which the builder can start
			/// writing memory
			VkBuffer resultBuffer, /// Result buffer storing the acceleration structure
			VkDeviceMemory resultMem,
			VkBool32 updateOnly = VK_FALSE, /// If true, simply refit the existing acceleration structure
			VkAccelerationStructureNV previousResult = VK_NULL_HANDLE
			/// Optional previous acceleration structure, used
			/// if an iterative update is requested
		);

	private:
		/// Vertex buffer descriptors used to generate the AS
		std::vector<VkGeometryNV> m_vertexBuffers = {};

		/// Amount of temporary memory required by the builder
		VkDeviceSize m_scratchSizeInBytes = 0;

		/// Amount of memory required to store the AS
		VkDeviceSize m_resultSizeInBytes = 0;

		/// Flags for the builder, specifying whether to allow iterative updates, or
		/// when to perform an update
		VkBuildAccelerationStructureFlagsNV m_flags;
	};
} // namespace nv_helpers_vk
