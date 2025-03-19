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

*/


#include "VHHelper.h"
#include "VKHelpers.h"

#include "RaytracingPipelineGenerator.h"

#include <stdexcept>
#include <unordered_set>

namespace nv_helpers_vk
{
	//--------------------------------------------------------------------------------------------------
	//
	// Start the description of a hit group, that contains at least a closest hit shader, but may
	// also contain an intesection shader and a any-hit shader. The method outputs the index of the
	// created hit group
	uint32_t RayTracingPipelineGenerator::StartHitGroup()
	{
		if (m_isHitGroupOpen)
		{
			throw std::logic_error("Hit group already open");
		}

		VkRayTracingShaderGroupCreateInfoNV groupInfo;
		groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
		groupInfo.pNext = nullptr;
		groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
		groupInfo.generalShader = VK_SHADER_UNUSED_NV;
		groupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		groupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		groupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		m_shaderGroups.push_back(groupInfo);

		m_isHitGroupOpen = true;
		return m_currentGroupIndex;
	}

	//--------------------------------------------------------------------------------------------------
	//
	// Add a hit shader stage in the current hit group, where the stage can be
	// VK_SHADER_STAGE_ANY_HIT_BIT_NV, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV, or
	// VK_SHADER_STAGE_INTERSECTION_BIT_NV
	uint32_t RayTracingPipelineGenerator::AddHitShaderStage(VkShaderModule module,
		VkShaderStageFlagBits shaderStage)
	{
		if (!m_isHitGroupOpen)
		{
			throw std::logic_error("Cannot add hit stage in when no hit group open");
		}

		auto &group = m_shaderGroups[m_currentGroupIndex];

		switch (shaderStage)
		{
		case VK_SHADER_STAGE_ANY_HIT_BIT_NV:
			if (group.anyHitShader != VK_SHADER_UNUSED_NV)
			{
				throw std::logic_error("Any hit shader already specified for current hit group");
			}

			break;

		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV:
			if (group.closestHitShader != VK_SHADER_UNUSED_NV)
			{
				throw std::logic_error("Closest hit shader already specified for current hit group");
			}

			break;

		case VK_SHADER_STAGE_INTERSECTION_BIT_NV:
			if (group.intersectionShader != VK_SHADER_UNUSED_NV)
			{
				throw std::logic_error("Intersection shader already specified for current hit group");
			}

			break;

		default:
			throw std::logic_error("Invalid hit shader type");
		}

		VkPipelineShaderStageCreateInfo stageCreate;
		stageCreate.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageCreate.pNext = nullptr;
		stageCreate.stage = shaderStage;
		stageCreate.module = module;
		// This member has to be 'main', regardless of the actual entry point of the shader
		stageCreate.pName = "main";
		stageCreate.flags = 0;
		stageCreate.pSpecializationInfo = nullptr;

		m_shaderStages.emplace_back(stageCreate);
		uint32_t shaderIndex = static_cast<uint32_t>(m_shaderStages.size() - 1);

		switch (shaderStage)
		{
		case VK_SHADER_STAGE_ANY_HIT_BIT_NV:
			group.anyHitShader = shaderIndex;
			break;

		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV:
			group.closestHitShader = shaderIndex;
			break;

		case VK_SHADER_STAGE_INTERSECTION_BIT_NV:
			group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
			group.intersectionShader = shaderIndex;
			break;
		}

		return m_currentGroupIndex;
	}

	//--------------------------------------------------------------------------------------------------
	//
	// End the description of the hit group
	void RayTracingPipelineGenerator::EndHitGroup()
	{
		if (!m_isHitGroupOpen)
		{
			throw std::logic_error("No hit group open");
		}

		m_isHitGroupOpen = false;
		m_currentGroupIndex++;
	}

	//--------------------------------------------------------------------------------------------------
	//
	// Add a ray generation shader stage, and return the index of the created stage
	uint32_t RayTracingPipelineGenerator::AddRayGenShaderStage(VkShaderModule module)
	{
		if (m_isHitGroupOpen)
		{
			throw std::logic_error("Cannot add raygen stage in when hit group open");
		}

		VkPipelineShaderStageCreateInfo stageCreate;
		stageCreate.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageCreate.pNext = nullptr;
		stageCreate.stage = VK_SHADER_STAGE_RAYGEN_BIT_NV;
		stageCreate.module = module;
		// This member has to be 'main', regardless of the actual entry point of the shader
		stageCreate.pName = "main";
		stageCreate.flags = 0;
		stageCreate.pSpecializationInfo = nullptr;

		m_shaderStages.emplace_back(stageCreate);
		uint32_t shaderIndex = static_cast<uint32_t>(m_shaderStages.size() - 1);

		VkRayTracingShaderGroupCreateInfoNV groupInfo;
		groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
		groupInfo.pNext = nullptr;
		groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
		groupInfo.generalShader = shaderIndex;
		groupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		groupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		groupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		m_shaderGroups.emplace_back(groupInfo);

		return m_currentGroupIndex++;
	}

	//--------------------------------------------------------------------------------------------------
	//
	// Add a miss shader stage, and return the index of the created stage
	uint32_t RayTracingPipelineGenerator::AddMissShaderStage(VkShaderModule module)
	{
		if (m_isHitGroupOpen)
		{
			throw std::logic_error("Cannot add miss stage in when hit group open");
		}

		VkPipelineShaderStageCreateInfo stageCreate;
		stageCreate.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageCreate.pNext = nullptr;
		stageCreate.stage = VK_SHADER_STAGE_MISS_BIT_NV;
		stageCreate.module = module;
		// This member has to be 'main', regardless of the actual entry point of the shader
		stageCreate.pName = "main";
		stageCreate.flags = 0;
		stageCreate.pSpecializationInfo = nullptr;

		m_shaderStages.emplace_back(stageCreate);
		uint32_t shaderIndex = static_cast<uint32_t>(m_shaderStages.size() - 1);

		VkRayTracingShaderGroupCreateInfoNV groupInfo;
		groupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
		groupInfo.pNext = nullptr;
		groupInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
		groupInfo.generalShader = shaderIndex;
		groupInfo.closestHitShader = VK_SHADER_UNUSED_NV;
		groupInfo.anyHitShader = VK_SHADER_UNUSED_NV;
		groupInfo.intersectionShader = VK_SHADER_UNUSED_NV;
		m_shaderGroups.emplace_back(groupInfo);

		return m_currentGroupIndex++;
	}

	//--------------------------------------------------------------------------------------------------
	//
	// Upon hitting a surface, a closest hit shader can issue a new TraceRay call. This parameter
	// indicates the maximum level of recursion. Note that this depth should be kept as low as
	// possible, typically 2, to allow hit shaders to trace shadow rays. Recursive ray tracing
	// algorithms must be flattened to a loop in the ray generation program for best performance.
	void RayTracingPipelineGenerator::SetMaxRecursionDepth(uint32_t
		maxDepth)
	{
		m_maxRecursionDepth = maxDepth;
	}

	//--------------------------------------------------------------------------------------------------
	//
	// Compiles the raytracing state object
	VkResult RayTracingPipelineGenerator::Generate(VkDevice device,
		VkPipelineLayout &pipelineLayout,
		VkPipeline *pipeline)
	{
		// Assemble the shader stages and recursion depth info into the raytracing pipeline
		VkRayTracingPipelineCreateInfoNV rayPipelineInfo;
		rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
		rayPipelineInfo.pNext = nullptr;
		rayPipelineInfo.flags = 0;
		rayPipelineInfo.stageCount = static_cast<uint32_t>(m_shaderStages.size());
		rayPipelineInfo.pStages = m_shaderStages.data();
		rayPipelineInfo.groupCount = static_cast<uint32_t>(m_shaderGroups.size());
		rayPipelineInfo.pGroups = m_shaderGroups.data();
		rayPipelineInfo.maxRecursionDepth = m_maxRecursionDepth;
		rayPipelineInfo.layout = pipelineLayout;
		rayPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		rayPipelineInfo.basePipelineIndex = 0;

		return vkCreateRayTracingPipelinesNV(device, nullptr, 1, &rayPipelineInfo, nullptr, pipeline);
	}

} // namespace nv_helpers_vk
