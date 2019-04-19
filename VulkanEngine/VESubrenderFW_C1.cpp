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
		VESubrender::initSubrenderer();

		VkDescriptorSetLayout perObjectLayout = getRendererForwardPointer()->getDescriptorSetLayoutPerObject();
		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
			{ perObjectLayout, perObjectLayout, getRendererForwardPointer()->getDescriptorSetLayoutShadow(), perObjectLayout },
			{ },
			&m_pipelineLayout);

		m_pipelines.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
			{ "shader/Forward/C1/vert.spv", "shader/Forward/C1/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout, getRendererForwardPointer()->getRenderPass(),
			{},
			&m_pipelines[0]);

	}
}


