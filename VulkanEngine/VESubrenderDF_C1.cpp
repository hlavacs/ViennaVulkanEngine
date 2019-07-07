/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"
#include "VESubrenderDF_C1.h"
#include "VERendererDeferred.h"


namespace ve {

	/**
	* \brief Initialize the subrenderer
	*
	* Create descriptor set layout, pipeline layout and the PSO
	*
	*/
	void VESubrenderDF_C1::initSubrenderer() {
        VESubrender::initSubrenderer();

        VkDescriptorSetLayout perObjectLayout = getRendererDeferredPointer()->getDescriptorSetLayoutPerObject();
        vh::vhPipeCreateGraphicsPipelineLayout(getRendererDeferredPointer()->getDevice(),
            { perObjectLayout, perObjectLayout, getRendererDeferredPointer()->getDescriptorSetLayoutShadow(), perObjectLayout },
            { },
            &m_pipelineLayout);

        m_pipelines.resize(1);
        vh::vhPipeCreateGraphicsPipeline(getRendererDeferredPointer()->getDevice(),
            { "shader/Deferred/C1/vert.spv", "shader/Deferred/C1/frag.spv" },
            getRendererDeferredPointer()->getSwapChainExtent(),
            m_pipelineLayout, getRendererDeferredPointer()->getRenderPass(),
            {},
            &m_pipelines[0], 0, VK_CULL_MODE_BACK_BIT, 4);
	}
}


