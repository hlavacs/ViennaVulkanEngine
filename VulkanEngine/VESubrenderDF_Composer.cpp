/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"
#include "VERendererDeferred.h"

namespace ve
{
	/**
		* \brief Initialize the subrenderer
		*
		* Create descriptor set layout, pipeline layout and the PSO
		*
		*/
	void VESubrenderDF_Composer::initSubrenderer()
	{
		VESubrenderDF::initSubrenderer();

		//create descriptor for the Onscreen pass

		//Onscreen Pass:
		//
		//set 0...cam UBO
		//set 1...light UBO
		//set 2...shadow maps
		//set 3...position, normal, albedo map

		VkDescriptorSetLayout perObjectLayout = m_renderer.getDescriptorSetLayoutPerObject();
		VkDescriptorSetLayout offscreenLayout = m_renderer.getDescriptorSetLayoutOffscreen();

		vh::vhPipeCreateGraphicsPipelineLayout(m_renderer.getDevice(),
			{ perObjectLayout, perObjectLayout,
			 m_renderer.getDescriptorSetLayoutShadow(), offscreenLayout },
			{},
			&m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(m_renderer.getDevice(),
			{ "media/shader/Deferred/Composition/vert.spv",
			 "media/shader/Deferred/Composition/frag.spv" },
			m_renderer.getSwapChainExtent(),
			m_pipelineLayout, m_renderer.getRenderPassOnscreen(),
			{ VK_DYNAMIC_STATE_BLEND_CONSTANTS },
			&m_pipelines[0], VK_CULL_MODE_FRONT_BIT, 1);
	}

	/**
		*
		* \brief Add an entity to the list of associated entities.
		*
		* \param[in] pEntity Pointer to the entity to include into the list.
		*
		*/
	void VESubrenderDF_Composer::addEntity(VEEntity *pEntity)
	{
		assert("function is not allowed");
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
	void VESubrenderDF_Composer::bindDescriptorSetsPerFrame(VkCommandBuffer
		commandBuffer,
		uint32_t imageIndex,
		VECamera *pCamera,
		VELight *pLight,
		std::vector<VkDescriptorSet>
		descriptorSetsShadow)
	{
		//Onscreen Pass:
		//
		//set 0...cam UBO
		//set 1...light UBO
		//set 2...shadow maps
		//set 3...position, normal, albedo map

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
	void VESubrenderDF_Composer::bindOffscreenDescriptorSet(VkCommandBuffer
		commandBuffer,
		uint32_t imageIndex)
	{
		//set 3...per frame, includes position, normal and albedo buffers
		std::vector<VkDescriptorSet> sets = {
			m_renderer.getDescriptorSetsOffscreen()[imageIndex],
		};

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout,
			3, (uint32_t)sets.size(), sets.data(), 0, nullptr);
	}

	void VESubrenderDF_Composer::setDynamicPipelineState(VkCommandBuffer
		commandBuffer,
		uint32_t numPass)
	{
		float blendConstants[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		vkCmdSetBlendConstants(commandBuffer, blendConstants);
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
	void VESubrenderDF_Composer::draw(VkCommandBuffer commandBuffer,
		uint32_t imageIndex,
		uint32_t numPass,
		VECamera *pCamera,
		VELight *pLight,
		std::vector<VkDescriptorSet> descriptorSetsShadow)
	{
		bindPipeline(commandBuffer);
		setDynamicPipelineState(commandBuffer, numPass);
		bindDescriptorSetsPerFrame(commandBuffer, imageIndex, pCamera, pLight, descriptorSetsShadow);
		bindOffscreenDescriptorSet(commandBuffer, imageIndex);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	}

} // namespace ve
