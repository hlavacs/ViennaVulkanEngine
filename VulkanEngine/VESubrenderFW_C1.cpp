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
	void VESubrenderFW_C1::initSubrenderer() {
		VESubrenderFW::initSubrenderer();

		VkDescriptorSetLayout perObjectLayout2 = getRendererForwardPointer()->getDescriptorSetLayoutPerObject();

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
		{ perObjectLayout2, perObjectLayout2,  getRendererForwardPointer()->getDescriptorSetLayoutShadow(), perObjectLayout2 },
		{},&m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
		{ "media/shader/Forward/C1/vert.spv", "media/shader/Forward/C1/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPass(),
			{ },
			&m_pipelines[0]);

	}

}


