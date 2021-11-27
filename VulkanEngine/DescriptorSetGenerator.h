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

#pragma once

#include "vulkan/vulkan.h"

#include <array>
#include <unordered_map>

namespace nv_helpers_vk
{
	/// Helper class generating consistent descriptor pools, layouts and sets
	class DescriptorSetGenerator
	{
	public:
		/// Add a binding to the descriptor set
		void AddBinding(uint32_t binding, /// Slot to which the descriptor will be bound, corresponding to the layout
						/// index in the shader
			uint32_t descriptorCount, /// Number of descriptors to bind
			VkDescriptorType type, /// Type of the bound descriptor(s)
			VkShaderStageFlags stage, /// Shader stage at which the bound resources will be available
			VkSampler *sampler = nullptr /// Corresponding sampler, in case of textures
		);

		/// Once the bindings have been added, this generates the descriptor pool with enough space to
		/// handle all the bound resources and allocate up to maxSets descriptor sets
		VkDescriptorPool GeneratePool(VkDevice device, uint32_t maxSets = 1);

		/// Once the bindings have been added, this generates the descriptor layout corresponding to the
		/// bound resources
		VkDescriptorSetLayout GenerateLayout(VkDevice device);

		/// Generate a descriptor set from the pool and layout
		VkDescriptorSet GenerateSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout);

		/// Store the information to write into one descriptor set entry: the number of descriptors of the
		/// entry, and where in the descriptor the buffer information should be written
		template <typename T, uint32_t offset>
		struct WriteInfo
		{
			/// Write descriptors
			std::vector<VkWriteDescriptorSet> writeDesc;
			/// Contents to write in one of the info members of the descriptor
			std::vector<std::vector<T>> contents;

			/// Since the VkWriteDescriptorSet structure requires pointers to the info descriptors, and we
			/// use std::vector to store those, the pointers can be set only when we are finished adding
			/// data in the vectors. The SetPointers then writes the info descriptor at the proper offset in
			/// the VkWriteDescriptorSet structure
			void SetPointers()
			{
				for (size_t i = 0; i < writeDesc.size(); i++)
				{
					T **dest = reinterpret_cast<T **>(reinterpret_cast<uint8_t *>(&writeDesc[i]) + offset);
					*dest = contents[i].data();
				}
			}

			/// Bind a vector of info descriptors to a slot in the descriptor set
			void Bind(VkDescriptorSet set, /// Target descriptor set
				uint32_t binding, /// Slot in the descriptor set the infos will be bound to
				VkDescriptorType type, /// Type of the descriptor
				const std::vector<T> &info /// Descriptor infos to bind
			)
			{
				// Initialize the descriptor write, keeping all the resource pointers to NULL since they will
				// be set by SetPointers once all resources have been bound
				VkWriteDescriptorSet descriptorWrite = {};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = set;
				descriptorWrite.dstBinding = binding;
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = type;
				descriptorWrite.descriptorCount = static_cast<uint32_t>(info.size());
				descriptorWrite.pBufferInfo = VK_NULL_HANDLE;
				descriptorWrite.pImageInfo = VK_NULL_HANDLE;
				descriptorWrite.pTexelBufferView = VK_NULL_HANDLE;
				descriptorWrite.pNext = VK_NULL_HANDLE;

				// If the binding point had already been used in a Bind call, replace the binding info
				// Linear search, not so great - hopefully not too many binding points
				for (size_t i = 0; i < writeDesc.size(); i++)
				{
					if (writeDesc[i].dstBinding == binding)
					{
						writeDesc[i] = descriptorWrite;
						contents[i] = info;
						return;
					}
				}
				// Add the write descriptor and resource info for later actual binding
				writeDesc.push_back(descriptorWrite);
				contents.push_back(info);
			}
		};

		/// Bind a buffer
		void Bind(VkDescriptorSet set,
			VkDescriptorType type,
			uint32_t binding,
			const std::vector<VkDescriptorBufferInfo> &bufferInfo);

		/// Bind an image
		void Bind(VkDescriptorSet set,
			VkDescriptorType type,
			uint32_t binding,
			const std::vector<VkDescriptorImageInfo> &imageInfo);

		/// Bind an acceleration structure
		void Bind(VkDescriptorSet set,
			VkDescriptorType type,
			uint32_t binding,
			const std::vector<VkWriteDescriptorSetAccelerationStructureNV> &accelInfo);

		/// Actually write the binding info into the descriptor set
		void UpdateSetContents(VkDevice device, VkDescriptorSet set);

	private:
		/// Association of the binding slot index with the binding information
		std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings;

		/// Buffer binding requests. Buffer descriptor infos are written into the pBufferInfo member of
		/// the VkWriteDescriptorSet structure
		WriteInfo<VkDescriptorBufferInfo, offsetof(VkWriteDescriptorSet, pBufferInfo)> m_buffers;
		/// Image binding requests. Image descriptor infos are written into the pImageInfo member of
		/// the VkWriteDescriptorSet structure
		WriteInfo<VkDescriptorImageInfo, offsetof(VkWriteDescriptorSet, pImageInfo)> m_images;
		/// Acceleration structure binding requests. Since this is using an non-core extension, AS
		/// descriptor infos are written into the pNext member of the VkWriteDescriptorSet structure
		WriteInfo<VkWriteDescriptorSetAccelerationStructureNV, offsetof(VkWriteDescriptorSet, pNext)>
			m_accelerationStructures;
	};

} // namespace nv_helpers_vk
