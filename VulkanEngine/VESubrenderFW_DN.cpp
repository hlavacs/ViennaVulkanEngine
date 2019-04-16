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

		VkDescriptorSetLayout perObjectLayout = getRendererForwardPointer()->getDescriptorSetLayoutPerObject();

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
			{ perObjectLayout, perObjectLayout,  getRendererForwardPointer()->getDescriptorSetLayoutShadow(), perObjectLayout, m_descriptorSetLayoutResources },
			{ },
			&m_pipelineLayout);

		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
			"shader/Forward/DN/vert.spv", "shader/Forward/DN/frag.spv",
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPass(),
			{ VK_DYNAMIC_STATE_BLEND_CONSTANTS },
			&m_pipeline);
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

		/*vh::vhBufCreateUniformBuffers(getRendererForwardPointer()->getVmaAllocator(),
			(uint32_t)getRendererForwardPointer()->getSwapChainNumber(),
			(uint32_t)sizeof(veUBOPerObject),
			pEntity->m_uniformBuffers, pEntity->m_uniformBuffersAllocation);

		vh::vhRenderCreateDescriptorSets(getRendererForwardPointer()->getDevice(),
			(uint32_t)getRendererForwardPointer()->getSwapChainNumber(),
			m_descriptorSetLayoutUBO,
			getRendererForwardPointer()->getDescriptorPool(),
			pEntity->m_descriptorSetsUBO);

		for (uint32_t i = 0; i < pEntity->m_descriptorSetsUBO.size(); i++) {
			vh::vhRenderUpdateDescriptorSet(getRendererForwardPointer()->getDevice(),
				pEntity->m_descriptorSetsUBO[i],
				{ pEntity->m_uniformBuffers[i] }, //UBOs
				{ sizeof(veUBOPerObject) },	//UBO sizes
				{ { VK_NULL_HANDLE } },	//textureImageViews
				{ { VK_NULL_HANDLE } }	//samplers
			);
		}
		*/

		vh::vhRenderCreateDescriptorSets(getRendererForwardPointer()->getDevice(),
			(uint32_t)getRendererForwardPointer()->getSwapChainNumber(),
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


