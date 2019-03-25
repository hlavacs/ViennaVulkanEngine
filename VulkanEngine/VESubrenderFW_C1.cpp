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

		//per object resources, set 0
		vh::vhRenderCreateDescriptorSetLayout(getRendererForwardPointer()->getDevice(),
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
			{ VK_SHADER_STAGE_VERTEX_BIT,	    },
			&m_descriptorSetLayoutUBO);

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
			{ getRendererForwardPointer()->getDescriptorSetLayoutPerFrame(), m_descriptorSetLayoutUBO, getRendererForwardPointer()->getDescriptorSetLayoutShadow() },
			{ },
			&m_pipelineLayout);

		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
			"shader/Forward/C1/vert.spv", "shader/Forward/C1/frag.spv",
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPass(),
			&m_pipeline);

	}

	/**
	* \brief Add an entity to the subrenderer
	*
	* Create a UBO for this entity, a descriptor set per swapchain image, and update the descriptor sets
	*
	*/
	void VESubrenderFW_C1::addEntity(VEEntity *pEntity) {
		VESubrender::addEntity(pEntity);

		vh::vhBufCreateUniformBuffers(getRendererForwardPointer()->getVmaAllocator(),
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
				{ {VK_NULL_HANDLE} },	//textureImageViews
				{ {VK_NULL_HANDLE} }	//samplers
			);
		}
	}
}


