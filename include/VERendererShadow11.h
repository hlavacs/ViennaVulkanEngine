#pragma once

namespace vve
{

	static constexpr uint32_t SHADOW_MAP_DIMENSION = 1024 * 2;
	static constexpr uint32_t SHADOW_MAX_NUM_LAYERS = 128*6;

	struct ShadowImage {
		vvh::Image shadowImage;
		uint32_t numberImageArraylayers{ 0 };
		uint32_t maxImageDimension2D{ 0 };
		uint32_t maxImageArrayLayers{ 0 };
		VkImageView m_cubeArrayView{ VK_NULL_HANDLE };
		VkImageView m_2DArrayView{ VK_NULL_HANDLE };
		std::vector<glm::mat4> m_lightSpaceMatrices;
	};

	using ShadowMaphandle = vsty::strong_type_t<ShadowImage, vsty::counter<>>;

    class RendererShadow11 : public Renderer {
	private:
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

		struct oShadowDescriptor {
			vvh::DescriptorSet m_oShadowDescriptor;
		};

		struct DummyImage {
			VkImage         m_dummyImage{ VK_NULL_HANDLE };
			VmaAllocation   m_dummyImageAllocation{ VK_NULL_HANDLE };
			VkImageView     m_dummyImageView{ VK_NULL_HANDLE };
		};

	public:
		RendererShadow11(const std::string& systemName, Engine& engine, const std::string& windowName);
        virtual ~RendererShadow11();

    private:
		bool OnInit(const Message& message);
		bool OnPrepareNextFrame(const Message& message);
		bool OnRecordNextFrame(const Message& message);
		bool OnObjectCreate(Message& message);
		bool OnObjectDestroy(Message& message);
		bool OnQuit(const Message& message);

		template<typename T>
		auto CountShadows(const uint32_t& num) const -> uint32_t;
		void CreateShadowMap();

		void SetDummyCubeArrayView();
		void SetDummy2DArrayView();

		void DestroyShadowMap();
		void RenderShadowMap(const VkCommandBuffer& cmdBuffer, uint32_t& layer);
		void RenderPointLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near = 1.0f, const float& far = 25.0f);
		void RenderDirectLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near = 1.0f, const float& far = 25.0f);
		void RenderSpotLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near = 1.0f, const float& far = 25.0f);
	
	private:
	    VkRenderPass m_renderPass{ VK_NULL_HANDLE };

		std::vector<VkImageView> m_layerViews{ VK_NULL_HANDLE };
		std::vector<VkFramebuffer> m_shadowFrameBuffers{ VK_NULL_HANDLE };

	    VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
		std::vector<VkCommandPool> m_commandPools{ VK_NULL_HANDLE };
		std::vector<VkCommandBuffer> m_commandBuffers{ VK_NULL_HANDLE };

		VkDescriptorSetLayout m_descriptorSetLayoutPerObject{ VK_NULL_HANDLE };
		vvh::Pipeline m_shadowPipeline{};

		vecs::Handle m_shadowImageHandle;

		State m_state = State::STATE_NEW;

		DummyImage m_dummyImage;
	};

};   // namespace vve

