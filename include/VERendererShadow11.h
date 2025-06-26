#pragma once

namespace vve
{

	static const uint32_t SHADOW_MAP_DIMENSION = 1024;
	static const uint32_t SHADOW_MAX_MAPS_PER_ROW = 1;
	static const uint32_t SHADOW_MAX_NUM_LAYERS = 128*6;
	

	struct ShadowImage {
		uint32_t maxImageDimension2D;
		uint32_t maxImageArrayLayers;
		uint32_t numberImageArraylayers{0};
		//std::vector<vvh::Image> shadowImages;
		// TODO: one image but needs multiple views for 1.1
		vvh::Image shadowImage;
		VkImageView m_cubeArrayView;
		uint32_t MaxNumberMapsUV() { return maxImageDimension2D / SHADOW_MAP_DIMENSION; };
		uint32_t MaxNumberMapsPerLayer() { return MaxNumberMapsUV() * MaxNumberMapsUV(); };
		uint32_t MaxNumberMapsPerImage() { return maxImageArrayLayers * MaxNumberMapsPerLayer(); };
		uint32_t NumberMapsPerImage() { return numberImageArraylayers * MaxNumberMapsPerLayer(); };
	};

	// TODO: move into shadowImage struct?
	struct oShadowDescriptor {
		vvh::DescriptorSet m_oShadowDescriptor;
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
		bool OnObjectCreate(Message message);
		bool OnObjectDestroy(Message message);
        bool OnQuit(Message message);
		template<typename T>
		uint32_t  CountShadows(uint32_t num);
		void CheckShadowMaps( uint32_t number);

		void DestroyShadowMap();
		void RenderPointLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near = 1.0f, const float& far = 25.0f);
		void RenderSpotLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near = 1.0f, const float& far = 25.0f);

	    VkRenderPass m_renderPass;

		bool m_renderedAlready = false;	// TODO: logic so that shadow maps dont get rendererd each frame!

		//VkImageView m_cubeArrayView{};
		//vecs::Handle m_ShadowCubeArrayViewHandle;
		std::vector<VkImageView> m_layerViews{};
		std::vector<VkFramebuffer> m_shadowFrameBuffers{};

	    VkDescriptorPool m_descriptorPool;    
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;

		vvh::Buffer m_uniformBuffersPerFrame;
		vvh::Buffer m_storageBuffersLights;
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame{ VK_NULL_HANDLE };
		VkDescriptorSetLayout m_descriptorSetLayoutPerObject{ VK_NULL_HANDLE };
		vvh::DescriptorSet m_descriptorSetPerFrame{0};
		vvh::Pipeline m_shadowPipeline;		

		vecs::Handle m_shadowImageHandle;
		uint32_t m_pass;
		uint32_t m_numberPasses{1};

		glm::ivec3 m_numberLightsPerType{0,0,0};

		struct PushConstantShadow {
			glm::mat4 lightSpaceMatrix;
			glm::vec3 lightPosition;
			float padding{ 0 };
		};
	};

};   // namespace vve

