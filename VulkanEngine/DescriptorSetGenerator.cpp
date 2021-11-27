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
#include "DescriptorSetGenerator.h"

namespace nv_helpers_vk
{
	//--------------------------------------------------------------------------------------------------
	// Add a binding to the descriptor set
	void DescriptorSetGenerator::AddBinding(
		uint32_t binding,
		uint32_t descriptorCount,
		VkDescriptorType type,
		VkShaderStageFlags stage,
		VkSampler *sampler)
	{
		VkDescriptorSetLayoutBinding b = {};
		b.binding = binding;
		b.descriptorCount = descriptorCount;
		b.descriptorType = type;
		b.pImmutableSamplers = sampler;
		b.stageFlags = stage;

		// Sanity check to avoid binding different resources to the same binding point
		if (m_bindings.find(binding) != m_bindings.end())
		{
			throw std::logic_error("Binding collision");
		}
		m_bindings[binding] = b;
	}

	//--------------------------------------------------------------------------------------------------
	// Once the bindings have been added, this generates the descriptor pool with enough space to
	// handle all the bound resources and allocate up to maxSets descriptor sets
	VkDescriptorPool DescriptorSetGenerator::GeneratePool(VkDevice device, uint32_t maxSets /* = 1 */)
	{
		VkDescriptorPool pool;

		// Aggregate the bindings to obtain the required size of the descriptors using that layout
		std::vector<VkDescriptorPoolSize> counters;
		counters.reserve(m_bindings.size());
		for (const auto &b : m_bindings)
		{
			counters.push_back({ b.second.descriptorType, b.second.descriptorCount });
		}

		// Create the pool information descriptor, that contains the number of descriptors of each type
		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(counters.size());
		poolInfo.pPoolSizes = counters.data();
		poolInfo.maxSets = maxSets;

		// Create the actual descriptor pool
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor pool!");
		}
		return pool;
	}

	//--------------------------------------------------------------------------------------------------
	// Once the bindings have been added, this generates the descriptor layout corresponding to the
	// bound resources
	VkDescriptorSetLayout DescriptorSetGenerator::GenerateLayout(VkDevice device)
	{
		VkDescriptorSetLayout layout;

		// Build the vector of bindings
		// For production, this copy should be avoided
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.reserve(m_bindings.size());
		for (const auto &b : m_bindings)
		{
			bindings.push_back(b.second);
		}

		// Create the layout from the vector of bindings
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create descriptor set layout!");
		}
		return layout;
	}

	//--------------------------------------------------------------------------------------------------
	// Generate a descriptor set from the pool and layout
	VkDescriptorSet DescriptorSetGenerator::GenerateSet(VkDevice device,
		VkDescriptorPool pool,
		VkDescriptorSetLayout layout)
	{
		VkDescriptorSet set;

		VkDescriptorSetLayout layouts[] = { layout };
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layouts;

		if (vkAllocateDescriptorSets(device, &allocInfo, &set) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate descriptor set!");
		}
		return set;
	}

	//--------------------------------------------------------------------------------------------------
	// Bind an buffer
	void DescriptorSetGenerator::Bind(VkDescriptorSet set,
		VkDescriptorType type,
		uint32_t binding,
		const std::vector<VkDescriptorBufferInfo> &bufferInfo)
	{
		m_buffers.Bind(set, binding, type, bufferInfo);
	}

	//--------------------------------------------------------------------------------------------------
	// Bind an image
	void DescriptorSetGenerator::Bind(VkDescriptorSet set,
		VkDescriptorType type,
		uint32_t binding,
		const std::vector<VkDescriptorImageInfo> &imageInfo)
	{
		m_images.Bind(set, binding, type, imageInfo);
	}

	//--------------------------------------------------------------------------------------------------
	// Bind an acceleration structure
	void DescriptorSetGenerator::Bind(
		VkDescriptorSet set,
		VkDescriptorType type,
		uint32_t binding,
		const std::vector<VkWriteDescriptorSetAccelerationStructureNV> &accelInfo)
	{
		m_accelerationStructures.Bind(set, binding, type, accelInfo);
	}

	//--------------------------------------------------------------------------------------------------
	// Actually write the binding info into the descriptor set
	void DescriptorSetGenerator::UpdateSetContents(VkDevice device, VkDescriptorSet set)
	{
		// For each resource type, set the actual pointers in the VkWriteDescriptorSet structures, and
		// write the resulting structures into the descriptor set
		m_buffers.SetPointers();
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(m_buffers.writeDesc.size()),
			m_buffers.writeDesc.data(), 0, nullptr);

		m_images.SetPointers();
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(m_images.writeDesc.size()),
			m_images.writeDesc.data(), 0, nullptr);

		m_accelerationStructures.SetPointers();
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(m_accelerationStructures.writeDesc.size()),
			m_accelerationStructures.writeDesc.data(), 0, nullptr);
	}

} // namespace nv_helpers_vk
