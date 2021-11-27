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
	void VESubrenderFW_C1::initSubrenderer()
	{
		VESubrenderFW::initSubrenderer();

		VkDescriptorSetLayout perObjectLayout = m_renderer.getDescriptorSetLayoutPerObject();

		vh::vhPipeCreateGraphicsPipelineLayout(m_renderer.getDevice(),
			{ perObjectLayout, perObjectLayout,
			 m_renderer.getDescriptorSetLayoutShadow(), perObjectLayout },
			{}, &m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(m_renderer.getDevice(),
			{ "media/shader/Forward/C1/vert.spv", "media/shader/Forward/C1/frag.spv" },
			m_renderer.getSwapChainExtent(),
			m_pipelineLayout, m_renderer.getRenderPass(),
			{},
			&m_pipelines[0]);
	}

} // namespace ve
