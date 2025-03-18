#pragma once

namespace vve
{

	struct ShadowMap {
		std::vector<vh::Map> m_shadowMaps;
	};

    class RendererShadow11 : public Renderer {

	public:
		RendererShadow11(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererShadow11();

    private:
        bool OnInit(Message message);
        bool OnPrepareNextFrame(Message message);
        bool OnRecordNextFrame(Message message);
        bool OnQuit(Message message);
		void CheckShadowMaps( vecs::Handle handle,  uint32_t number);

	    VkRenderPass m_renderPass;
	    VkDescriptorPool m_descriptorPool;    
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;

		vh::UniformBuffers m_uniformBuffersPerFrame;
		vh::UniformBuffers m_uniformBuffersLights;
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
		vh::DescriptorSet m_descriptorSetPerFrame{0};

		VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
		vh::Pipeline m_graphicsPipeline;		

		size_t m_maxNumberLights{16};
		glm::ivec3 m_numberLightsPerType{0,0,0};
	};

};   // namespace vve

