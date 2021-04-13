/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"
#include "VESubrenderDF_Shadow.h"
#include "VERendererDeferred.h"


namespace ve {

	/**
	* \brief Initialize the subrenderer
	*
	* Create descriptor set layout, pipeline layout and the PSO
	*
	*/
	void VESubrenderDF_Shadow::initSubrenderer() {
		VESubrender::initSubrenderer();


        VkDescriptorSetLayout perObjectLayout = m_renderer.getDescriptorSetLayoutPerObject();
        vh::vhPipeCreateGraphicsPipelineLayout(m_renderer.getDevice(),
            { perObjectLayout, perObjectLayout, m_renderer.getDescriptorSetLayoutShadow(), perObjectLayout },
            { },
            &m_pipelineLayout);

        m_pipelines.resize(1);
        vh::vhPipeCreateGraphicsShadowPipeline(m_renderer.getDevice(),
            "media/shader/Deferred/Shadow/vert.spv",
			m_renderer.getShadowMapExtent(),
            m_pipelineLayout, m_renderer.getRenderPassShadow(),
            &m_pipelines[0]);
	}

	/**
	*
	* \brief Add an entity to the list of associated entities.
	*
	* \param[in] pEntity Pointer to the entity to include into the list.
	*
	*/
	void VESubrenderDF_Shadow::addEntity(VEEntity *pEntity) {
		m_entities.push_back(pEntity);
	}

	/**
	*
	* \brief Bind default descriptor sets - 0...per object 1...per frame
	*
	* The function binds the default descriptor sets -  0...per object 1...per frame.
	* Can be overloaded.
	*
	* \param[in] commandBuffer The command buffer to record into all draw calls
	* \param[in] imageIndex Index of the current swap chain image
	* \param[in] entity Pointer to the entity to draw
	*
	*/
    void VESubrenderDF_Shadow::bindDescriptorSetsPerEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity) {
		//set 0...per frame, includes cam and shadow matrices
		//set 1...per object UBO

		std::vector<VkDescriptorSet> sets = { entity->m_memoryHandle.pMemBlock->descriptorSets[imageIndex] };

		uint32_t offset = entity->m_memoryHandle.entryIndex * sizeof(VEEntity::veUBOPerEntity_t);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
			3, (uint32_t)sets.size(), sets.data(), 1, &offset);
	}


	/**
	* \brief Draw all associated entities for the shadow pass
	*
	* The subrenderer maintains a list of all associated entities. In this function it goes through all of them
	* and draws them. A vector is used in order to be able to parallelize this in case thousands or objects are in the list
	*
	* \param[in] commandBuffer The command buffer to record into all draw calls
	* \param[in] imageIndex Index of the current swap chain image
	*
	*/
    void VESubrenderDF_Shadow::draw(VkCommandBuffer commandBuffer, uint32_t imageIndex, uint32_t numPass,
        VECamera *pCamera, VELight *pLight,
        std::vector<VkDescriptorSet> descriptorSetsShadow) {

        bindPipeline(commandBuffer);

        bindDescriptorSetsPerFrame(commandBuffer, imageIndex, pCamera, pLight, descriptorSetsShadow);

		//go through all entities and draw them
		for (auto subrender : getSubrenderers()) {
			for (auto pEntity : subrender->getEntities()) {
				if (pEntity->m_castsShadow) {
					bindDescriptorSetsPerEntity(commandBuffer, imageIndex, pEntity);	//bind the entity's descriptor sets
					drawEntity(commandBuffer, imageIndex, pEntity);
				}
			}
		}
	}



}

