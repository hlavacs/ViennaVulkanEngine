/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"



namespace ve {
	
	/**
	* \brief If the window size changes then some resources have to be recreated to fit the new size.
	*/
	void VESubrender::recreateResources() {
		closeSubrenderer();
		initSubrenderer();
	}

	/**
	* \brief Close down the subrenderer and destroy all local resources.
	*/
	void VESubrender::closeSubrenderer() {
		if( m_pipeline!=VK_NULL_HANDLE )
			vkDestroyPipeline(getRendererPointer()->getDevice(), m_pipeline, nullptr);

		if (m_pipelineLayout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(getRendererPointer()->getDevice(), m_pipelineLayout, nullptr);

		if (m_descriptorSetLayoutResources != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(getRendererPointer()->getDevice(), m_descriptorSetLayoutResources, nullptr);
	}


	/**
	* \brief Bind the subrenderer's pipeline to a commandbuffer
	*/
	void VESubrender::bindPipeline(VkCommandBuffer commandBuffer ) {
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);	//bind the PSO
	}


	void VESubrender::bindDescriptorSets(	VkCommandBuffer commandBuffer, uint32_t imageIndex, 
											VECamera *pCamera, VELight *pLight, VkDescriptorSet descriptorSetShadow ) {

		//set 0...cam UBO
		//set 1...per object UBO
		//set 2...shadow maps
		//set 3...additional per object resources
		//set 4...light resources

		std::vector<VkDescriptorSet> set = { pCamera->m_descriptorSetsUBO[imageIndex] };
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, (uint32_t)set.size(), set.data(), 0, nullptr);

		if (descriptorSetShadow != VK_NULL_HANDLE) {
			set = { descriptorSetShadow };
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 2, (uint32_t)set.size(), set.data(), 0, nullptr);
		}

		set = { pLight->m_descriptorSetsUBO[imageIndex] };
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 4, (uint32_t)set.size(), set.data(), 0, nullptr);
	}


	/**
	*
	* \brief Bind default descriptor sets
	*
	* The function binds the default descriptor sets. Can be overloaded.
	*
	* \param[in] commandBuffer The command buffer to record into all draw calls
	* \param[in] imageIndex Index of the current swap chain image
	* \param[in] entity Pointer to the entity to draw
	*
	*/
	void VESubrender::bindDescriptorSets(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity) {

		//set 0...cam UBO (set before)
		//set 1...per object UBO
		//set 2...shadow map
		//set 3...additional per object resources
		//set 4...light resources (set before)

		std::vector<VkDescriptorSet> sets = { entity->m_descriptorSetsUBO[imageIndex] };
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, (uint32_t)sets.size(), sets.data(), 0, nullptr);

		if (entity->m_descriptorSetsResources.size()>0) {
			sets = { entity->m_descriptorSetsResources[imageIndex] };
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 3, (uint32_t)sets.size(), sets.data(), 0, nullptr);
		}
	}





	/**
	* \brief Draw all associated entities.
	*
	* The subrenderer maintains a list of all associated entities. In this function it goes through all of them
	* and draws them. A vector is used in order to be able to parallelize this in case thousands or objects are in the list
	*
	* \param[in] commandBuffer The command buffer to record into all draw calls
	* \param[in] imageIndex Index of the current swap chain image
	*
 	*/
	void VESubrender::draw(VkCommandBuffer commandBuffer, uint32_t imageIndex ) {
		//go through all entities and draw them
		for (auto pEntity : m_entities) {
			if (pEntity->m_drawEntity ) {
				drawEntity(commandBuffer, imageIndex, pEntity);
			}
		}
	}

	/**
	*
	* \brief Draw one entity
	*
	* The function binds the vertex buffer, index buffer, and descriptor set of the entity, then commits a draw call 
	*
	* \param[in] commandBuffer The command buffer to record into all draw calls
	* \param[in] imageIndex Index of the current swap chain image
	* \param[in] entity Pointer to the entity to draw
	*
	*/
	void VESubrender::drawEntity(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity) {

		bindDescriptorSets(commandBuffer, imageIndex, entity);					//bind the entity's descriptor sets

		VkBuffer vertexBuffers[] = { entity->m_pMesh->m_vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);	//bind vertex buffer

		vkCmdBindIndexBuffer(commandBuffer, entity->m_pMesh->m_indexBuffer, 0, VK_INDEX_TYPE_UINT32); //bind index buffer

		vkCmdDrawIndexed(commandBuffer, entity->m_pMesh->m_indexCount, 1, 0, 0, 0); //record the draw call
	}


	/**
	*
	* \brief Add an entity to the list of associated entities.
	*
	* \param[in] pEntity Pointer to the entity to include into the list.
	*
	*/
	void VESubrender::addEntity(VEEntity *pEntity) {
		m_entities.push_back(pEntity);
		pEntity->m_pSubrenderer = this;
	}

	/**
	*
	* \brief Removes an entity from this subrenderer - does NOT delete it
	*
	* Since we use indices and each entity knows its onw index, this is an O(1) operation.
	*
	* \param[in] pEntity Pointer to the entity to be removed
	*
	*/
	void VESubrender::removeEntity(VEEntity *pEntity) {

		uint32_t size = (uint32_t)m_entities.size();
		if (size == 0) return;

		for (uint32_t i = 0; i < size; i++) {
			if (m_entities[i] == pEntity) {
				m_entities[i] = m_entities[size - 1];			//replace with former last entity (could be identical)
				m_entities.pop_back();							//remove the last
			}
		}
	}
}

