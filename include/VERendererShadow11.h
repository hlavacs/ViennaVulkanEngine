#pragma once

namespace vve
{

	struct ShadowMap {
		std::vector<vh::Map> m_shadowMaps;
	};

    class RendererShadow11 : public Renderer {

		static const int MAP_DIMENSION = 1024;


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

		vh::Buffer m_uniformBuffersPerFrame;
		vh::Buffer m_uniformBuffersLights;
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
		vh::DescriptorSet m_descriptorSetPerFrame{0};

		VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
		vh::Pipeline m_graphicsPipeline;		

		size_t m_maxNumberLights{128};
		glm::ivec3 m_numberLightsPerType{0,0,0};
	};

};   // namespace vve

