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
	void VESubrenderFW_C1::initSubrenderer() {
		VESubrender::initSubrenderer();

		VkDescriptorSetLayout perObjectLayout = getRendererForwardPointer()->getDescriptorSetLayoutPerObject();
		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
			{ perObjectLayout, perObjectLayout, getRendererForwardPointer()->getDescriptorSetLayoutShadow(), perObjectLayout },
			{ },&m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
			{ "shader/Forward/C1/vert.spv", "shader/Forward/C1/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPass(),
			{},
			&m_pipelines[0]);


		//-----------------------------------------------------------------
		VkDescriptorSetLayout perObjectLayout2 = getRendererForwardPointer()->getDescriptorSetLayoutPerObject2();

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
		{ perObjectLayout, perObjectLayout,  getRendererForwardPointer()->getDescriptorSetLayoutShadow(), perObjectLayout2 },
		{},&m_pipelineLayout2);

		m_pipelines2.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
		{ "shader/Forward/C1/vert.spv", "shader/Forward/C1/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout2, getRendererForwardPointer()->getRenderPass(),
			{ },
			&m_pipelines2[0]);

	}


	void VESubrenderFW_C1::bindPipeline(VkCommandBuffer commandBuffer) {
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines2[0]);	//bind the PSO
	}

	void VESubrenderFW_C1::bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity) {

		std::vector<VkDescriptorSet> sets = { entity->m_memoryHandle.pMemBlock->descriptorSets[imageIndex] };
		if (entity->m_descriptorSetsResources.size() > 0) {
			sets.push_back(entity->m_descriptorSetsResources[imageIndex]);
		}

		uint32_t offset = entity->m_memoryHandle.entryIndex * sizeof(VEEntity::veUBOPerObject_t);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout2,
			3, (uint32_t)sets.size(), sets.data(), 1, &offset);
	}
}


