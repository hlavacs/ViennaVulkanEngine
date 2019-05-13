/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"


namespace ve {

	/**
	* \brief Initialize the subrenderer
	*
	* Create descriptor set layout, pipeline layout and the PSO
	*
	*/
	void VESubrenderFW_DN::initSubrenderer() {

		VESubrender::initSubrenderer();

		vh::vhRenderCreateDescriptorSetLayout(getRendererForwardPointer()->getDevice(),
			{ m_resourceArrayLength,						m_resourceArrayLength },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_FRAGMENT_BIT,					VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutResources);

		VkDescriptorSetLayout perObjectLayout = getRendererForwardPointer()->getDescriptorSetLayoutPerObject2();

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
		{ perObjectLayout, perObjectLayout,  getRendererForwardPointer()->getDescriptorSetLayoutShadow(), 
			perObjectLayout, m_descriptorSetLayoutResources },
		{},
			&m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
		{ "shader/Forward/DN/vert.spv", "shader/Forward/DN/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPass(),
			{ VK_DYNAMIC_STATE_BLEND_CONSTANTS },
			&m_pipelines[0]);

		//----------------------------------------

		if (m_maps.empty()) m_maps.resize(2);
	}


	void VESubrenderFW_DN::setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass) {
		if (numPass == 0) {
			float blendConstants[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			vkCmdSetBlendConstants(commandBuffer, blendConstants);
			return;
		}

		float blendConstants[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		vkCmdSetBlendConstants(commandBuffer, blendConstants);
	}

	void VESubrenderFW_DN::bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity) {

		//set 0...cam UBO
		//set 1...light resources
		//set 2...shadow maps
		//set 3...per object UBO
		//set 4...additional per object resources

		std::vector<VkDescriptorSet> sets = { entity->m_memoryHandle.pMemBlock->descriptorSets[imageIndex] };
		if (m_descriptorSetsResources.size() > 0) {
			sets.push_back(m_descriptorSetsResources[entity->getResourceIdx() / m_resourceArrayLength]);
		}

		uint32_t offset = entity->m_memoryHandle.entryIndex * sizeof(VEEntity::veUBOPerObject_t);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
			3, (uint32_t)sets.size(), sets.data(), 1, &offset);

	}



	/**
	* \brief Add an entity to the subrenderer
	*
	* Create a UBO for this entity, a descriptor set per swapchain image, and update the descriptor sets
	*
	*/
	void VESubrenderFW_DN::addEntity(VEEntity *pEntity) {

		VkDescriptorImageInfo imageInfo1 = {};
		imageInfo1.imageView = pEntity->m_pMaterial->mapDiffuse->m_imageView;
		imageInfo1.sampler = pEntity->m_pMaterial->mapDiffuse->m_sampler;
		imageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo imageInfo2 = {};
		imageInfo2.imageView = pEntity->m_pMaterial->mapNormal->m_imageView;
		imageInfo2.sampler = pEntity->m_pMaterial->mapNormal->m_sampler;
		imageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		pEntity->setResourceIdx((uint32_t)m_entities.size());

		uint32_t offset = 0;
		if (pEntity->getResourceIdx() % m_resourceArrayLength == 0) {
			vh::vhRenderCreateDescriptorSets(getRendererForwardPointer()->getDevice(),
				1, m_descriptorSetLayoutResources,
				getRendererForwardPointer()->getDescriptorPool(),
				m_descriptorSetsResources);

			offset = (uint32_t)m_maps[0].size();
			m_maps[0].resize(offset + m_resourceArrayLength);
			for (uint32_t i = offset; i<offset + m_resourceArrayLength; i++) m_maps[0][i] = imageInfo1;
			m_maps[1].resize(offset + m_resourceArrayLength);
			for (uint32_t i = offset; i<offset + m_resourceArrayLength; i++) m_maps[1][i] = imageInfo2;
		}
		else {
			offset = (uint32_t)m_maps[0].size() / m_resourceArrayLength - 1;
			offset *= m_resourceArrayLength;
			m_maps[0][m_entities.size()] = imageInfo1;
			m_maps[1][m_entities.size()] = imageInfo2;
		}

		vh::vhRenderUpdateDescriptorSetMaps(getRendererPointer()->getDevice(),
											m_descriptorSetsResources[m_descriptorSetsResources.size() - 1],
											0, offset, m_resourceArrayLength, m_maps);

		VESubrender::addEntity(pEntity);

	}

}


