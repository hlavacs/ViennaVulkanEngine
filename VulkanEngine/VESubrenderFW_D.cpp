/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VEInclude.h"
#include "VERendererForward.h"

namespace ve
{
	/**
		* \brief Initialize the subrenderer
		*
		* Create descriptor set layout, pipeline layout and the PSO
		*
		*/
	void VESubrenderFW_D::initSubrenderer()
	{
		VESubrenderFW::initSubrenderer();

		vh::vhRenderCreateDescriptorSetLayout(m_renderer.getDevice(),
			{ m_resourceArrayLength },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutResources);

		VkDescriptorSetLayout perObjectLayout = m_renderer.getDescriptorSetLayoutPerObject();

		vh::vhPipeCreateGraphicsPipelineLayout(m_renderer.getDevice(),
			{ perObjectLayout, perObjectLayout,
			 m_renderer.getDescriptorSetLayoutShadow(),
			 perObjectLayout, m_descriptorSetLayoutResources },
			{}, &m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(m_renderer.getDevice(),
			{ "media/shader/Forward/D/vert.spv", "media/shader/Forward/D/frag.spv" },
			m_renderer.getSwapChainExtent(),
			m_pipelineLayout, m_renderer.getRenderPass(),
			{ VK_DYNAMIC_STATE_BLEND_CONSTANTS },
			&m_pipelines[0]);

		if (m_maps.empty())
			m_maps.resize(1);
	}

	/**
		* \brief Set the danymic pipeline stat, i.e. the blend constants to be used
		*
		* \param[in] commandBuffer The currently used command buffer
		* \param[in] numPass The current pass number - in the forst pass, write over pixel colors, after this add pixel colors
		*
		*/
	void VESubrenderFW_D::setDynamicPipelineState(VkCommandBuffer
		commandBuffer,
		uint32_t numPass)
	{
		if (numPass == 0)
		{
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
	void VESubrenderFW_D::addEntity(VEEntity *pEntity)
	{
		std::vector<VkDescriptorImageInfo> maps = { pEntity->m_pMaterial->mapDiffuse->m_imageInfo };

		addMaps(pEntity, maps);

		VESubrender::addEntity(pEntity);
	}

} // namespace ve
