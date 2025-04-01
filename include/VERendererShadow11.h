#pragma once

namespace vve
{

	static const uint32_t SHADOW_MAP_DIMENSION = 1024;

	struct ShadowImage {
		uint32_t maxImageDimension2D;
		uint32_t maxImageArrayLayers;
		uint32_t numberImageArraylayers;
		std::vector<vh::Map> shadowImages;
		uint32_t MaxNumberMapsPerLayer() { return maxImageDimension2D / SHADOW_MAP_DIMENSION; };
		uint32_t MaxNumberMapsPerImage() { return maxImageArrayLayers * MaxNumberMapsPerLayer(); };
		uint32_t NumberMapsPerImage() { return numberImageArraylayers * MaxNumberMapsPerLayer(); };
	};

	using ShadowMaphandle = vsty::strong_type_t<ShadowImage, vsty::counter<>>;

    class RendererShadow11 : public Renderer {

	public:
		RendererShadow11(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererShadow11();

    private:
        bool OnInit(Message message);
        bool OnPrepareNextFrame(Message message);
        bool OnRecordNextFrame(Message message);
        bool OnQuit(Message message);
		template<typename T>
		void RegisterForLight(int& i);
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

		size_t m_pass;
		glm::ivec3 m_numberLightsPerType{0,0,0};
	};

};   // namespace vve

