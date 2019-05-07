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
			{ 1,											1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,	VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_FRAGMENT_BIT,					VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutResources);

		VkDescriptorSetLayout perObjectLayout2 = getRendererForwardPointer()->getDescriptorSetLayoutPerObject2();

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
		{ perObjectLayout2, perObjectLayout2,  getRendererForwardPointer()->getDescriptorSetLayoutShadow(), perObjectLayout2, m_descriptorSetLayoutResources },
		{},
			&m_pipelineLayout2);

		m_pipelines2.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
		{ "shader/Forward/D/vert.spv", "shader/Forward/D/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout2, getRendererForwardPointer()->getRenderPass(),
			{ VK_DYNAMIC_STATE_BLEND_CONSTANTS },
			&m_pipelines2[0]);

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

	/**
	* \brief Add an entity to the subrenderer
	*
	* Create a UBO for this entity, a descriptor set per swapchain image, and update the descriptor sets
	*
	*/
	void VESubrenderFW_DN::addEntity(VEEntity *pEntity) {
		VESubrender::addEntity(pEntity);

		vh::vhRenderCreateDescriptorSets(getRendererForwardPointer()->getDevice(),
			1, //(uint32_t)getRendererForwardPointer()->getSwapChainNumber(),
			m_descriptorSetLayoutResources,
			getRendererForwardPointer()->getDescriptorPool(),
			pEntity->m_descriptorSetsResources);

		for (uint32_t i = 0; i < pEntity->m_descriptorSetsResources.size(); i++) {
			vh::vhRenderUpdateDescriptorSet(getRendererForwardPointer()->getDevice(),
				pEntity->m_descriptorSetsResources[i],
				{ VK_NULL_HANDLE, VK_NULL_HANDLE }, //UBOs
				{ 0,              0 },	//UBO sizes
				{ {pEntity->m_pMaterial->mapDiffuse->m_imageView}, {pEntity->m_pMaterial->mapNormal->m_imageView} },	//textureImageViews
				{ {pEntity->m_pMaterial->mapDiffuse->m_sampler},   {pEntity->m_pMaterial->mapNormal->m_sampler} }	//samplers
			);
		}
	}

}


