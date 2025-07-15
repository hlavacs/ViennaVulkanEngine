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
		// TODO: one image but needs multiple views for 1.1
		vvh::Image shadowImage;
		VkImageView m_cubeArrayView;
		VkImageView m_2DArrayView;
		std::vector<glm::mat4> m_lightSpaceMatrices;
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
		RendererShadow11(const std::string& systemName, Engine& engine, const std::string& windowName);
        virtual ~RendererShadow11();

    private:
        bool OnInit(Message message);
        bool OnPrepareNextFrame(Message message);
        bool OnRecordNextFrame(Message message);
		bool OnObjectCreate(Message message);
		bool OnObjectDestroy(Message message);
        bool OnQuit(Message message);

		template<typename T>
		auto CountShadows(const uint32_t& num) const -> uint32_t;
		void CreateShadowMap();

		void DestroyShadowMap();
		void RenderShadowMap(const VkCommandBuffer& cmdBuffer, uint32_t& layer);
		void RenderPointLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near = 1.0f, const float& far = 25.0f);
		void RenderDirectLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near = 1.0f, const float& far = 25.0f);
		void RenderSpotLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near = 1.0f, const float& far = 25.0f);

	    VkRenderPass m_renderPass{ VK_NULL_HANDLE };;

		std::vector<VkImageView> m_layerViews{ VK_NULL_HANDLE };
		std::vector<VkFramebuffer> m_shadowFrameBuffers{ VK_NULL_HANDLE };

	    VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
	    VkCommandPool m_commandPool{ VK_NULL_HANDLE };
		std::vector<VkCommandBuffer> m_commandBuffers{ VK_NULL_HANDLE };

		VkDescriptorSetLayout m_descriptorSetLayoutPerObject{ VK_NULL_HANDLE };
		vvh::Pipeline m_shadowPipeline;		

		vecs::Handle m_shadowImageHandle;

		struct PushConstantShadow {
			glm::mat4 lightSpaceMatrix;
			glm::vec3 lightPosition;
			float padding{ 0 };
		};

		enum class State : int {
			STATE_NEW,
			STATE_PREPARED,
			STATE_RECORDED,
		};
		State m_state = State::STATE_NEW;

		struct DummyImage {
			VkImage         m_dummyImage{ VK_NULL_HANDLE };
			VmaAllocation   m_dummyImageAllocation{ VK_NULL_HANDLE };
			VkImageView     m_dummyImageView{ VK_NULL_HANDLE };
		};
		DummyImage m_dummyImage;
	};

};   // namespace vve

