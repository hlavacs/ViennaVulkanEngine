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
	void VESubrenderFW_Cubemap::initSubrenderer() {
		VESubrender::initSubrenderer();

		vh::vhRenderCreateDescriptorSetLayout(getRendererForwardPointer()->getDevice(),
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutResources);

		VkDescriptorSetLayout perObjectLayout = getRendererForwardPointer()->getDescriptorSetLayoutPerObject();
		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
			{ perObjectLayout, perObjectLayout, getRendererForwardPointer()->getDescriptorSetLayoutShadow(), perObjectLayout, m_descriptorSetLayoutResources },
			{ },
			&m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
			{ "shader/Forward/Cubemap/vert.spv", "shader/Forward/Cubemap/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPass(),
			{},
			&m_pipelines[0]);

		//-----------------------------------------------------------------
		VkDescriptorSetLayout perObjectLayout2 = getRendererForwardPointer()->getDescriptorSetLayoutPerObject2();

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
		{ perObjectLayout2, perObjectLayout2,  getRendererForwardPointer()->getDescriptorSetLayoutShadow(), perObjectLayout2, m_descriptorSetLayoutResources },
		{}, &m_pipelineLayout2);

		m_pipelines2.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
		{ "shader/Forward/Cubemap/vert.spv", "shader/Forward/Cubemap/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout2, getRendererForwardPointer()->getRenderPass(),
			{ },
			&m_pipelines2[0]);


	}

	/**
	* \brief Add an entity to the subrenderer
	*
	* Create a UBO for this entity, a descriptor set per swapchain image, and update the descriptor sets
	*
	*/
	void VESubrenderFW_Cubemap::addEntity(VEEntity *pEntity) {
		VESubrender::addEntity(pEntity);

		vh::vhRenderCreateDescriptorSets(getRendererForwardPointer()->getDevice(),
			(uint32_t)getRendererPointer()->getSwapChainNumber(),
			m_descriptorSetLayoutResources,
			getRendererForwardPointer()->getDescriptorPool(),
			pEntity->m_descriptorSetsResources);

		for (uint32_t i = 0; i < pEntity->m_descriptorSetsResources.size(); i++) {
			vh::vhRenderUpdateDescriptorSet(getRendererForwardPointer()->getDevice(),
				pEntity->m_descriptorSetsResources[i],
				{ VK_NULL_HANDLE }, //UBOs
				{ 0  }, //UBO sizes
				{ {pEntity->m_pMaterial->mapDiffuse->m_imageView} },	//textureImageViews
				{ {pEntity->m_pMaterial->mapDiffuse->m_sampler} }	//samplers
			);
		}

	}

	/*void VESubrenderFW_Cubemap::bindPipeline(VkCommandBuffer commandBuffer) {
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines2[0]);	//bind the PSO
	}

	void VESubrenderFW_Cubemap::bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity) {

		std::vector<VkDescriptorSet> sets = { entity->m_memoryHandle.pMemBlock->descriptorSets[imageIndex] };
		if (entity->m_descriptorSetsResources.size() > 0) {
			sets.push_back(entity->m_descriptorSetsResources[imageIndex]);
		}

		uint32_t offset = entity->m_memoryHandle.entryIndex * sizeof(VEEntity::veUBOPerObject_t);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout2,
			3, (uint32_t)sets.size(), sets.data(), 1, &offset);
	}*/

}


