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

		vh::vhRenderCreateDescriptorSetLayout(getRendererForwardPointer()->getDevice(),
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutResources);

		vh::vhRenderCreateDescriptorSetLayout(getRendererForwardPointer()->getDevice(),
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutResources2);

		VkDescriptorSetLayout perObjectLayout = getRendererForwardPointer()->getDescriptorSetLayoutPerObject2();

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
		{	perObjectLayout, perObjectLayout,  getRendererForwardPointer()->getDescriptorSetLayoutShadow(), 
			perObjectLayout, m_descriptorSetLayoutResources, m_descriptorSetLayoutResources2 },
		{}, &m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
			{ "shader/Forward/D/vert.spv", "shader/Forward/D/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPass(),
			{ VK_DYNAMIC_STATE_BLEND_CONSTANTS },
			&m_pipelines[0]);

		//----------------------------------------

		vh::vhRenderCreateDescriptorSets(getRendererForwardPointer()->getDevice(),
			1, 
			m_descriptorSetLayoutResources2,
			getRendererForwardPointer()->getDescriptorPool(),
			m_descriptorSetsResources);

		if( m_diffuseMaps.size()>0)
			vh::vhRenderUpdateDescriptorSetMaps(getRendererPointer()->getDevice(),
												m_descriptorSetsResources[0], 0, m_diffuseMaps);


	}

	void VESubrenderFW_D::setDynamicPipelineState(VkCommandBuffer commandBuffer, uint32_t numPass) {
		if (numPass == 0 ) {
			float blendConstants[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			vkCmdSetBlendConstants(commandBuffer, blendConstants);
			return;
		}

		float blendConstants[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		vkCmdSetBlendConstants(commandBuffer, blendConstants);
	}


	void VESubrenderFW_D::bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity) {

		//set 0...cam UBO
		//set 1...light resources
		//set 2...shadow maps
		//set 3...per object UBO
		//set 4...additional per object resources

		std::vector<VkDescriptorSet> sets = { entity->m_memoryHandle.pMemBlock->descriptorSets[imageIndex] };
		if (entity->m_descriptorSetsResources.size() > 0) {
			sets.push_back(entity->m_descriptorSetsResources[0]);
		}
		if (m_descriptorSetsResources.size() > 0) {
			sets.push_back(m_descriptorSetsResources[0]);
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
	void VESubrenderFW_D::addEntity(VEEntity *pEntity) {
		VESubrender::addEntity( pEntity);

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageView = pEntity->m_pMaterial->mapDiffuse->m_imageView;
		imageInfo.sampler = pEntity->m_pMaterial->mapDiffuse->m_sampler;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		pEntity->setResourceIdx( (uint32_t)m_diffuseMaps.size());
		m_diffuseMaps.push_back(imageInfo);

		vh::vhRenderUpdateDescriptorSetMaps(getRendererPointer()->getDevice(),
											m_descriptorSetsResources[0], 0, m_diffuseMaps);

		//----------------------------------

		vh::vhRenderCreateDescriptorSets(getRendererForwardPointer()->getDevice(),
			1, //(uint32_t)getRendererForwardPointer()->getSwapChainNumber(),
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


