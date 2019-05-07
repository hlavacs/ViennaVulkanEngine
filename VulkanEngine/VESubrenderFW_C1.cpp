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

		VkDescriptorSetLayout perObjectLayout2 = getRendererForwardPointer()->getDescriptorSetLayoutPerObject2();

		vh::vhPipeCreateGraphicsPipelineLayout(getRendererForwardPointer()->getDevice(),
		{ perObjectLayout2, perObjectLayout2,  getRendererForwardPointer()->getDescriptorSetLayoutShadow(), perObjectLayout2 },
		{},&m_pipelineLayout2);

		m_pipelines2.resize(1);
		vh::vhPipeCreateGraphicsPipeline(getRendererForwardPointer()->getDevice(),
		{ "shader/Forward/C1/vert.spv", "shader/Forward/C1/frag.spv" },
			getRendererForwardPointer()->getSwapChainExtent(),
			m_pipelineLayout2, getRendererForwardPointer()->getRenderPass(),
			{ },
			&m_pipelines2[0]);

	}

}


