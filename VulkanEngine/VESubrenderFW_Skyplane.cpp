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
	void VESubrenderFW_Skyplane::initSubrenderer()
	{
		VESubrender::initSubrenderer();

		vh::vhRenderCreateDescriptorSetLayout(m_renderer.getDevice(),
			{ m_resourceArrayLength },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ VK_SHADER_STAGE_FRAGMENT_BIT },
			&m_descriptorSetLayoutResources);

		VkDescriptorSetLayout perObjectLayout = m_renderer.getDescriptorSetLayoutPerObject();

		vh::vhPipeCreateGraphicsPipelineLayout(m_renderer.getDevice(),
			{ perObjectLayout, perObjectLayout,
			 m_renderer.getDescriptorSetLayoutShadow(), perObjectLayout,
			 m_descriptorSetLayoutResources },
			{}, &m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(m_renderer.getDevice(),
			{ "media/shader/Forward/Skyplane/vert.spv",
			 "media/shader/Forward/Skyplane/frag.spv" },
			m_renderer.getSwapChainExtent(),
			m_pipelineLayout, m_renderer.getRenderPass(),
			{},
			&m_pipelines[0]);

		if (m_maps.empty())
			m_maps.resize(1);
	}

	/**
		* \brief Add an entity to the subrenderer
		*
		* Create a UBO for this entity, a descriptor set per swapchain image, and update the descriptor sets
		*
		*/
	void VESubrenderFW_Skyplane::addEntity(VEEntity *pEntity)
	{
		std::vector<VkDescriptorImageInfo> maps = { pEntity->m_pMaterial->mapDiffuse->m_imageInfo };

		addMaps(pEntity, maps);

		VESubrender::addEntity(pEntity);
	}

} // namespace ve
