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
	void VESubrenderFW_Shadow::initSubrenderer() {
		VESubrender::initSubrenderer();

		//per object resources, set 0
		/*vh::vhRenderCreateDescriptorSetLayout(getRendererForwardPointer()->getDevice(),
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
			{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, },
			&m_descriptorSetLayoutUBO);
		*/

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
			{ VESceneObject::m_descriptorSetLayoutPerObject, VESceneObject::m_descriptorSetLayoutPerObject, VK_NULL_HANDLE, VESceneObject::m_descriptorSetLayoutPerObject },
			{ { VK_SHADER_STAGE_VERTEX_BIT, 0, 4 } },
			&m_pipelineLayout);

		vh::vhPipeCreateGraphicsShadowPipeline(getRendererForwardPointer()->getDevice(),
			"shader/Forward/Shadow/vert.spv", 
			getRendererForwardPointer()->getShadowMapExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPassShadow(),
			&m_pipeline);
	}

	/**
	*
	* \brief Add an entity to the list of associated entities.
	*
	* \param[in] pEntity Pointer to the entity to include into the list.
	*
	*/
	void VESubrenderFW_Shadow::addEntity(VEEntity *pEntity) {
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
	void VESubrenderFW_Shadow::bindDescriptorSets(VkCommandBuffer commandBuffer, uint32_t imageIndex, VEEntity *entity) {
		//set 0...cam UBO
		//set 1...light UBO
		//set 2...shadow maps
		//set 3...per object UBO
		//set 4...additional per object resources

		std::vector<VkDescriptorSet> sets =
		{
			entity->m_descriptorSetsUBO[imageIndex]
		};

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 3, (uint32_t)sets.size(), sets.data(), 0, nullptr);
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
	void VESubrenderFW_Shadow::draw(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
		//go through all entities and draw them
		for (auto object : getSceneManagerPointer()->m_sceneNodes) {
			VESceneNode *pObject = object.second;
			if (pObject->getNodeType() == VESceneNode::VE_OBJECT_TYPE_ENTITY) {
				VEEntity *pEntity = (VEEntity*)pObject;

				if (pEntity->m_drawEntity && pEntity->m_castsShadow) {
					drawEntity(commandBuffer, imageIndex, pEntity);
				}
			}
		}
	}



}

