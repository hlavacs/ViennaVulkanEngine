/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"

namespace ve
{
	/**
		* \brief Initialize the subrenderer
		*/
	void VESubrenderFW::initSubrenderer() {};

	/**
		* \brief If the window size changes then some resources have to be recreated to fit the new size.
		*/
	void VESubrenderFW::recreateResources()
	{
		closeSubrenderer();
		initSubrenderer();

		uint32_t size = (uint32_t)m_descriptorSetsResources.size();
		if (size > 0)
		{
			m_descriptorSetsResources.clear();
			vh::vhRenderCreateDescriptorSets(m_renderer.getDevice(),
				size, m_descriptorSetLayoutResources,
				m_renderer.getDescriptorPool(),
				m_descriptorSetsResources);

			for (uint32_t i = 0; i < size; i++)
			{
				vh::vhRenderUpdateDescriptorSetMaps(m_renderer.getDevice(),
					m_descriptorSetsResources[i],
					0, i * m_resourceArrayLength, m_resourceArrayLength, m_maps);
			}
		}
	}

	/**
		* \brief Close down the subrenderer and destroy all local resources.
		*/
	void VESubrenderFW::closeSubrenderer()
	{
		for (auto pipeline : m_pipelines)
		{
			vkDestroyPipeline(m_renderer.getDevice(), pipeline, nullptr);
		}

		if (m_pipelineLayout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(m_renderer.getDevice(), m_pipelineLayout, nullptr);

		if (m_descriptorSetLayoutResources != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(m_renderer.getDevice(), m_descriptorSetLayoutResources, nullptr);
	}

	/**
		* \brief Bind the subrenderer's pipeline to a commandbuffer
		*
		* \param[in] commandBuffer The command buffer to bind the pipeline to
		*
		*/
	void VESubrenderFW::bindPipeline(VkCommandBuffer
		commandBuffer)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[0]); //bind the PSO
	}

	/**
	* \brief Bind per frame descriptor sets to the pipeline layout
	*
	* \param[in] commandBuffer The command buffer that is used for recording commands
	* \param[in] imageIndex The index of the swapchain image that is currently used
	* \param[in] pCamera Pointer to the current light camera
	* \param[in] pLight Pointer to the currently used light
	* \param[in] descriptorSetsShadow Shadow maps that are used for creating shadow
	*
	*/
	void VESubrenderFW::bindDescriptorSetsPerFrame(VkCommandBuffer
		commandBuffer,
		uint32_t imageIndex,
		VECamera *pCamera,
		VELight *pLight,
		std::vector<VkDescriptorSet>
		descriptorSetsShadow)
	{
		//set 0...cam UBO
		//set 1...light resources
		//set 2...shadow maps
		//set 3...per object UBO
		//set 4...additional per object resources

		std::vector<VkDescriptorSet> set = { pCamera->m_memoryHandle.pMemBlock->descriptorSets[imageIndex],
											pLight->m_memoryHandle.pMemBlock->descriptorSets[imageIndex] };

		uint32_t offsets[2] = { (uint32_t)(pCamera->m_memoryHandle.entryIndex * sizeof(VECamera::veUBOPerCamera_t)),
							   (uint32_t)(pLight->m_memoryHandle.entryIndex * sizeof(VELight::veUBOPerLight_t)) };

		if (descriptorSetsShadow.size() > 0)
		{
			set.push_back(descriptorSetsShadow[imageIndex]);
		}

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
			0, (uint32_t)set.size(), set.data(), 2, offsets);
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
	void VESubrenderFW::bindDescriptorSetsPerEntity(VkCommandBuffer
		commandBuffer,
		uint32_t imageIndex,
		VEEntity *entity)
	{
		//set 0...cam UBO
		//set 1...light resources
		//set 2...shadow maps
		//set 3...per object UBO
		//set 4...additional per object resources

		std::vector<VkDescriptorSet> sets = { entity->m_memoryHandle.pMemBlock->descriptorSets[imageIndex] };
		if (m_descriptorSetsResources.size() > 0 && entity->getResourceIdx() % m_resourceArrayLength == 0)
		{
			sets.push_back(m_descriptorSetsResources[entity->getResourceIdx() / m_resourceArrayLength]);
		}

		uint32_t offset = entity->m_memoryHandle.entryIndex * sizeof(VEEntity::veUBOPerEntity_t);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
			3, (uint32_t)sets.size(), sets.data(), 1, &offset);
	}

	/**
	* \brief Draw all associated entities.
	*
	* The subrenderer maintains a list of all associated entities. In this function it goes through all of them
	* and draws them. A vector is used in order to be able to parallelize this in case thousands or objects are in the list
	*
	* \param[in] commandBuffer The command buffer to record into all draw calls
	* \param[in] imageIndex Index of the current swap chain image
	* \param[in] numPass The number of the light that has been rendered
	* \param[in] pCamera Pointer to the current light camera
	* \param[in] pLight Pointer to the current light
	* \param[in] descriptorSetsShadow The shadow maps to be used.
	*
	*/
	void VESubrenderFW::draw(VkCommandBuffer commandBuffer,
		uint32_t imageIndex,
		uint32_t numPass,
		VECamera *pCamera,
		VELight *pLight,
		std::vector<VkDescriptorSet> descriptorSetsShadow)
	{
		if (m_entities.size() == 0)
			return;
		m_idxLastRecorded = (uint32_t)m_entities.size() - 1;

		if (numPass > 0 && getClass() != VE_SUBRENDERER_CLASS_OBJECT)
			return;

		bindPipeline(commandBuffer);

		setDynamicPipelineState(commandBuffer, numPass);

		bindDescriptorSetsPerFrame(commandBuffer, imageIndex, pCamera, pLight, descriptorSetsShadow);

		//go through all entities and draw them
		for (auto pEntity : m_entities)
		{
			bindDescriptorSetsPerEntity(commandBuffer, imageIndex, pEntity); //bind the entity's descriptor sets
			drawEntity(commandBuffer, imageIndex, pEntity);
		}
	}

	/**
	* \brief Remember the last recorded entity
	*
	* Needed for incremental recording.
	*
	*/
	void VESubrenderFW::afterDrawFinished()
	{
		m_idxLastRecorded = (uint32_t)m_entities.size() - 1;
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
	void VESubrenderFW::drawEntity(VkCommandBuffer commandBuffer,
		uint32_t imageIndex,
		VEEntity *entity)
	{
		VkBuffer vertexBuffers[] = { entity->m_pMesh->m_vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets); //bind vertex buffer

		vkCmdBindIndexBuffer(commandBuffer, entity->m_pMesh->m_indexBuffer, 0, VK_INDEX_TYPE_UINT32); //bind index buffer

		vkCmdDrawIndexed(commandBuffer, entity->m_pMesh->m_indexCount, 1, 0, 0, 0); //record the draw call
	}

	/**
	*
	* \brief Add some maps of an entity to the map list of this subrenderer
	*
	* m_maps is a list of map lists. Each entity may need 1-N maps, like diffuse, normal, etc.
	* m_maps thus has N entries, one for such a map type. m_map[0] might hold all diffuse maps.
	* m_map[1] might hold all normal maps. m_map[2] might hold all specular maps. etc.
	* If a new entity is added, then its maps are just appended to the N lists.
	* However, we have descriptor sets, each set having N slot bindings (one binding for each map type),
	* each binding describing an array of e.g. K textures.
	* So we break each list into chunks of size K, and bind each chunk to one descriptor.
	* Variable offset denotes the start of such a chunk.
	*
	* \param[in] pEntity Pointer to the entity
	* \param[in] newMaps List of 1..N maps to be added to this subrenderer, e.g. {diffuse texture}, or {diffuse tex, normal}
	*
	*/
	void VESubrenderFW::addMaps(VEEntity *pEntity, std::vector<VkDescriptorImageInfo> &newMaps)
	{
		pEntity->setResourceIdx((uint32_t)
			m_entities.size()); //index into the array of textures for this entity, shader will take remainder and not absolute value
		
		uint32_t offset = 0; //offset into the list of maps - index where a particular array starts
		if (pEntity->getResourceIdx() % m_resourceArrayLength == 0)
		{ //array is full or there is none yet? -> we need a new array
			vh::vhRenderCreateDescriptorSets(m_renderer.getDevice(),
				1,
				m_descriptorSetLayoutResources, //layout contains arrays of this size
				m_renderer.getDescriptorPool(),
				m_descriptorSetsResources);
			offset = (uint32_t)m_maps[0].size(); //number of maps of first bind slot, e.g. number of diffuse maps

			for (uint32_t i = 0; i < m_maps.size(); i++)
			{ //go through all map bind slots , e.g. first diffuse, then normal, then specular, then ...
				m_maps[i].resize(offset + m_resourceArrayLength); //make room for the new array elements
				for (uint32_t j = offset; j < offset + m_resourceArrayLength; j++) //have to fill the whole new array, even if there is only one map yet
					m_maps[i][j] = newMaps[i]; //fill the new array up with copies of the first elemenet
			}
		}
		else
		{ //we are inside an array, size is ALWAYS a multiple of m_resourceArrayLength!
			offset = (uint32_t)m_maps[0].size() / m_resourceArrayLength - 1; //get index of array
			offset *= m_resourceArrayLength; //get start index where the array starts
			for (uint32_t i = 0; i < m_maps.size(); i++)
			{ //go through all map types
				m_maps[i][m_entities.size()] = newMaps[i]; //copy the map into the array
			}
		}

		vh::vhRenderUpdateDescriptorSetMaps(m_renderer.getDevice(), //update the descriptor that holds the map array
			m_descriptorSetsResources[m_descriptorSetsResources.size() - 1],
			0,
			offset, //start offset of the current map arrays that should be updated
			m_resourceArrayLength,
			m_maps);
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
	void VESubrenderFW::removeEntity(VEEntity *pEntity)
	{
		uint32_t size = (uint32_t)m_entities.size();
		if (size == 0)
			return;

		for (uint32_t i = 0; i < size; i++)
		{
			if (m_entities[i] == pEntity)
			{
				//move the last entity and its maps to the place of the removed entity
				m_entities[i] = m_entities[size - 1]; //replace with former last entity (could be identical)

				if (m_maps.size() > 0)
				{ //are there maps?
					m_entities[i]->setResourceIdx(i); //new resource index
					for (uint32_t j = 0; j < m_maps.size(); j++)
					{ //move also the map entries
						m_maps[j][i] = m_maps[j][size - 1];
					}

					//update the descriptor set where the entity was removed
					uint32_t arrayIndex = (uint32_t)(i / m_resourceArrayLength);
					vh::vhRenderUpdateDescriptorSetMaps(m_renderer.getDevice(),
						m_descriptorSetsResources[arrayIndex],
						0,
						arrayIndex * m_resourceArrayLength,
						m_resourceArrayLength, m_maps);

					//shrink the lists
					m_entities.pop_back(); //remove the last
					if (m_entities.size() % m_resourceArrayLength == 0)
					{ //shrunk?
						for (uint32_t j = 0; j < m_maps.size(); j++)
						{
							m_maps[j].resize(m_entities.size()); //remove map entries
						}
						m_descriptorSetsResources.resize(m_entities.size() / m_resourceArrayLength); //remove descriptor sets
					}
				}
				else
				{
					m_entities.pop_back(); //remove the last
				}
				return;
			}
		}
	}

} // namespace ve
