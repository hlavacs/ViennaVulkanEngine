/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"
#include "VESubrenderDF_Cubemap2.h"
#include "VERendererDeferred.h"

namespace ve {

	/**
	* \brief Initialize the subrenderer
	*
	* Create descriptor set layout, pipeline layout and the PSO
	*
	*/
	void VESubrenderDF_Cubemap2::initSubrenderer() {
		VESubrender::initSubrenderer();

        vh::vhRenderCreateDescriptorSetLayout(getRendererDeferredPointer()->getDevice(),
            { 1 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
            { VK_SHADER_STAGE_FRAGMENT_BIT },
            &m_descriptorSetLayoutResources);

        VkDescriptorSetLayout perObjectLayout = getRendererDeferredPointer()->getDescriptorSetLayoutPerObject();
        vh::vhPipeCreateGraphicsPipelineLayout(getRendererDeferredPointer()->getDevice(),
            { perObjectLayout, perObjectLayout, getRendererDeferredPointer()->getDescriptorSetLayoutShadow(), perObjectLayout, m_descriptorSetLayoutResources },
            {},
            &m_pipelineLayout);

        m_pipelines.resize(1);
        vh::vhPipeCreateGraphicsPipeline(getRendererDeferredPointer()->getDevice(),
            { "shader/Deferred/Cubemap2/vert.spv", "shader/Deferred/Cubemap2/frag.spv" },
            getRendererDeferredPointer()->getSwapChainExtent(),
            m_pipelineLayout, getRendererDeferredPointer()->getRenderPass(),
            {},
            &m_pipelines[0], 0, VK_CULL_MODE_BACK_BIT, 4);
	}

	/**
	* \brief Add an entity to the subrenderer
	*
	* Create a UBO for this entity, a descriptor set per swapchain image, and update the descriptor sets
	*
	*/
	void VESubrenderDF_Cubemap2::addEntity(VEEntity *pEntity) {
		VESubrender::addEntity(pEntity);

        vh::vhRenderCreateDescriptorSets(getRendererDeferredPointer()->getDevice(),
            (uint32_t)getRendererPointer()->getSwapChainNumber(),
            m_descriptorSetLayoutResources,
            getRendererDeferredPointer()->getDescriptorPool(),
            pEntity->m_descriptorSetsResources);

        for(uint32_t i = 0; i < pEntity->m_descriptorSetsResources.size(); i++) {
            vh::vhRenderUpdateDescriptorSet(getRendererDeferredPointer()->getDevice(),
                pEntity->m_descriptorSetsResources[i],
                { VK_NULL_HANDLE }, //UBOs
                { 0 }, //UBO sizes
                { { pEntity->m_pMaterial->mapDiffuse->m_imageView } },	//textureImageViews
                { { pEntity->m_pMaterial->mapDiffuse->m_sampler } }	//samplers
            );
        }

	}
}


