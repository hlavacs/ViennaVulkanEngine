#pragma once

namespace vve {

	class RendererDeferred11 : public Renderer {

		friend class RendererDeferred;

		enum GBufferIndex { POSITION = 0, NORMALS = 1, ALBEDO = 2 };

	public:
		RendererDeferred11(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferred11();

	private:
		bool OnInit(Message message);
		bool OnPrepareNextFrame(Message message);
		bool OnRecordNextFrame(Message message);
		bool OnObjectCreate(Message message);
		bool OnObjectDestroy(Message message);
		bool OnQuit(Message message);
		void CreateGeometryPipeline();
		void CreateLightingPipeline();

		void getBindingDescription(int binding, int stride, auto& bdesc);
		auto getBindingDescriptions() -> std::vector<VkVertexInputBindingDescription>;
		void getAttributeDescription(int binding, int location, VkFormat format, auto& attd);
		auto getAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription>;
		template<typename T>
		auto RegisterLight(float type, std::vector<vh::Light>& lights, int& i) -> int;

		std::vector<VkFramebuffer> m_gBufferFrameBuffers{};

		vh::Buffer m_uniformBuffersPerFrame{};	// TODO: maybe rename
		vh::Buffer m_uniformBuffersLights{};

		// TODO: Maybe make GBufferAttachment struct for better alignment
		VkSampler m_sampler{ VK_NULL_HANDLE };
		std::array<vh::GBufferImage, 3> m_gBufferAttachments{};

		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame{ VK_NULL_HANDLE };
		VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
		vh::DescriptorSet m_descriptorSetComposition{};
		vh::DescriptorSet m_descriptorSetObject{};

		VkRenderPass m_geometryPass{ VK_NULL_HANDLE };
		VkRenderPass m_lightingPass{ VK_NULL_HANDLE };

		vh::Pipeline m_geometryPipeline{};
		vh::Pipeline m_lightingPipeline{};

		std::vector<VkCommandPool> m_commandPools{};
		std::vector<VkCommandBuffer> m_commandBuffers{};

		// TODO: maybe constexpr is not a good idea? -> also increase number later!!!
		static constexpr uint32_t m_maxNumberLights{ 16 };
		glm::ivec3 m_numberLightsPerType{ 0,0,0 };
	};

}	// namespace vve
