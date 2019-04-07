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
	void VESubrenderFW_D::initSubrenderer() {
		VESubrender::initSubrenderer();

		//per object resources, set 0
		/*vh::vhRenderCreateDescriptorSetLayout(getRendererForwardPointer()->getDevice(),
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
			{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutUBO);
			*/

		vh::vhRenderCreateDescriptorSetLayout(getRendererForwardPointer()->getDevice(),
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutResources);

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
			{ VESceneObject::m_descriptorSetLayoutPerObject, VESceneObject::m_descriptorSetLayoutPerObject,  getRendererForwardPointer()->getDescriptorSetLayoutShadow(), VESceneObject::m_descriptorSetLayoutPerObject, m_descriptorSetLayoutResources },
			{ },
			&m_pipelineLayout);

		vh::vhPipeCreateGraphicsPipeline(	getRendererForwardPointer()->getDevice(),
											"shader/Forward/D/vert.spv", "shader/Forward/D/frag.spv",
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
	void VESubrenderFW_D::addEntity(VEEntity *pEntity) {
		VESubrender::addEntity( pEntity);

		/*vh::vhBufCreateUniformBuffers(getRendererForwardPointer()->getVmaAllocator(),
			(uint32_t)getRendererForwardPointer()->getSwapChainNumber(),
			(uint32_t)sizeof(VESceneObject::veUBOPerObject_t),
			pEntity->m_uniformBuffers, pEntity->m_uniformBuffersAllocation);

		vh::vhRenderCreateDescriptorSets(getRendererForwardPointer()->getDevice(),
			(uint32_t)getRendererForwardPointer()->getSwapChainNumber(),
			m_descriptorSetLayoutUBO,
			getRendererForwardPointer()->getDescriptorPool(),
			pEntity->m_descriptorSetsUBO);

		for (uint32_t i = 0; i < pEntity->m_descriptorSetsUBO.size(); i++) {
			vh::vhRenderUpdateDescriptorSet(getRendererForwardPointer()->getDevice(),
				pEntity->m_descriptorSetsUBO[i],
				{ pEntity->m_uniformBuffers[i] },	//UBOs
				{ sizeof(veUBOPerObject) },					//UBO sizes
				{ {VK_NULL_HANDLE} },	//textureImageViews
				{ {VK_NULL_HANDLE} }	//samplers
			);
		}*/

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
				{ {pEntity->m_pMaterial->mapDiffuse->m_imageView} },	//textureImageViews
				{ {pEntity->m_pMaterial->mapDiffuse->m_sampler} }	//samplers
			);
		}
	}
}


