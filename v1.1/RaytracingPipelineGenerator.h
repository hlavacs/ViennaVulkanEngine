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

The raytracing pipeline combines the raytracing shaders into a state object,
that can be thought of as an executable GPU program. For that, it requires the
shaders compiled as DXIL libraries, where each library exports symbols in a way
similar to DLLs. Those symbols are then used to refer to these shaders libraries
when creating hit groups, associating the shaders to their root signatures and
declaring the steps of the pipeline. All the calls to this helper class can be
done in arbitrary order. Some basic sanity checks are also performed when
compiling in debug mode.

Simple usage of this class:

pipeline.AddLibrary(m_rayGenLibrary.Get(), {L"RayGen"});
pipeline.AddLibrary(m_missLibrary.Get(), {L"Miss"});
pipeline.AddLibrary(m_hitLibrary.Get(), {L"ClosestHit"});

pipeline.AddHitGroup(L"HitGroup", L"ClosestHit");



pipeline.SetMaxRecursionDepth(1);

rtStateObject = pipeline.Generate();

*/

#pragma once

#include "vulkan/vulkan.h"

#include <string>
#include <vector>

namespace nv_helpers_vk
{
	/// Helper class to create raytracing pipelines
	class RayTracingPipelineGenerator
	{
	public:
		/// Start the description of a hit group, that contains at least a closest hit shader, but may
		/// also contain an intesection shader and a any-hit shader. The method outputs the index of the
		/// created hit group
		uint32_t StartHitGroup();

		/// Add a hit shader stage in the current hit group, where the stage can be
		/// VK_SHADER_STAGE_ANY_HIT_BIT_NV, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, or
		/// VK_SHADER_STAGE_INTERSECTION_BIT_NV
		uint32_t AddHitShaderStage(VkShaderModule module, VkShaderStageFlagBits shaderStage);

		/// End the description of the hit group
		void EndHitGroup();

		/// Add a ray generation shader stage, and return the index of the created stage
		uint32_t AddRayGenShaderStage(VkShaderModule module);

		/// Add a miss shader stage, and return the index of the created stage
		uint32_t AddMissShaderStage(VkShaderModule module);

		/// Upon hitting a surface, a closest hit shader can issue a new TraceRay call. This parameter
		/// indicates the maximum level of recursion. Note that this depth should be kept as low as
		/// possible, typically 2, to allow hit shaders to trace shadow rays. Recursive ray tracing
		/// algorithms must be flattened to a loop in the ray generation program for best performance.
		void SetMaxRecursionDepth(uint32_t maxDepth);

		/// Compiles the raytracing state object
		VkResult Generate(VkDevice device,
			VkPipelineLayout &pipelineLayout,
			VkPipeline *pipeline);

	private:
		/// Shader stages contained in the pipeline
		std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

		/// Each shader stage belongs to a group. There are 3 group types: general, triangle hit and procedural hit.
		/// The general group type (VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV) is used for raygen, miss and callable shaders.
		/// The triangle hit group type (VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV) is used for closest hit and
		/// any hit shaders, when used together with the built-in ray-triangle intersection shader.
		/// The procedural hit group type (VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV) is used for custom
		/// intersection shaders, and also groups closest hit and any hit shaders that are used together with that intersection shader.
		std::vector<VkRayTracingShaderGroupCreateInfoNV> m_shaderGroups;

		/// Index of the current hit group
		uint32_t m_currentGroupIndex = 0;

		/// True if a group description is currently started
		bool m_isHitGroupOpen = false;

		/// Maximum recursion depth, initialized to 1 to at least allow tracing primary rays
		uint32_t m_maxRecursionDepth = 1;
	};

} // namespace nv_helpers_vk
