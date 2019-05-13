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
		m_resourceArrayLength = 16;

		VESubrender::initSubrenderer();

		vh::vhRenderCreateDescriptorSetLayout(getRendererForwardPointer()->getDevice(),
			{ m_resourceArrayLength },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutResources);

		VkDescriptorSetLayout perObjectLayout = getRendererForwardPointer()->getDescriptorSetLayoutPerObject2();

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
		{	perObjectLayout, perObjectLayout,  getRendererForwardPointer()->getDescriptorSetLayoutShadow(), 
			perObjectLayout, m_descriptorSetLayoutResources },
		{}, &m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
		{ "shader/Forward/Cubemap/vert.spv", "shader/Forward/Cubemap/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPass(),
			{ },
			&m_pipelines[0]);

		if (m_maps.empty()) m_maps.resize(1);

	}

	/**
	* \brief Add an entity to the subrenderer
	*
	* Create a UBO for this entity, a descriptor set per swapchain image, and update the descriptor sets
	*
	*/
	void VESubrenderFW_Cubemap::addEntity(VEEntity *pEntity) {

		VkDescriptorImageInfo imageInfo1 = {};
		imageInfo1.imageView = pEntity->m_pMaterial->mapDiffuse->m_imageView;
		imageInfo1.sampler = pEntity->m_pMaterial->mapDiffuse->m_sampler;
		imageInfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		std::vector<VkDescriptorImageInfo> maps = { imageInfo1 };

		addMaps(pEntity, maps);

		VESubrender::addEntity(pEntity);
	}

}


