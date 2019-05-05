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
	void VESubrenderFW_Skyplane::initSubrenderer() {
		VESubrender::initSubrenderer();

		vh::vhRenderCreateDescriptorSetLayout(getRendererForwardPointer()->getDevice(),
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutResources);

		VkDescriptorSetLayout perObjectLayout = getRendererForwardPointer()->getDescriptorSetLayoutPerObject();
		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
			{ perObjectLayout, perObjectLayout, getRendererForwardPointer()->getDescriptorSetLayoutShadow(), perObjectLayout, m_descriptorSetLayoutResources },
			{},
			&m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
			{ "shader/Forward/Skyplane/vert.spv", "shader/Forward/Skyplane/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPass(),
			{},
			&m_pipelines[0]);

	}

	/**
	* \brief Add an entity to the subrenderer
	*
	* Create a UBO for this entity, a descriptor set per swapchain image, and update the descriptor sets
	*
	*/
	void VESubrenderFW_Skyplane::addEntity(VEEntity *pEntity) {
		VESubrender::addEntity(pEntity);

		vh::vhRenderCreateDescriptorSets(getRendererForwardPointer()->getDevice(),
			(uint32_t)getRendererForwardPointer()->getSwapChainNumber(),
			m_descriptorSetLayoutResources,
			getRendererForwardPointer()->getDescriptorPool(),
			pEntity->m_descriptorSetsResources);

		for (uint32_t i = 0; i < pEntity->m_descriptorSetsResources.size(); i++) {
			vh::vhRenderUpdateDescriptorSet(getRendererForwardPointer()->getDevice(),
				pEntity->m_descriptorSetsResources[i],
				{ VK_NULL_HANDLE },	//UBOs
				{ 0 },					//UBO sizes
				{ { pEntity->m_pMaterial->mapDiffuse->m_imageView } },	//textureImageViews
				{ { pEntity->m_pMaterial->mapDiffuse->m_sampler } }	//samplers
			);
		}


	}
}


