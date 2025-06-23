#pragma once

namespace vve {

	template <typename Derived>
	class RendererDeferredCommon : public Renderer{

	protected:
		static constexpr uint32_t MAX_NUMBER_LIGHTS{ 128 };

		enum GBufferIndex : uint8_t { NORMAL = 0, ALBEDO = 1, METALLIC_ROUGHNESS = 2, DEPTH = 3, COUNT };

		struct PipelinePerType {
			std::string m_type;
			VkDescriptorSetLayout m_descriptorSetLayoutPerObject{};
			vvh::Pipeline m_graphicsPipeline{};
		};

		struct alignas(16) PushConstantsLight {
			alignas(16) glm::mat4 m_invViewProj;
		};

		struct alignas(16) PushConstantsMaterial {
			alignas(8) glm::vec2 m_metallRoughness{ 0.0f, 1.0f };
			alignas(4) float padding[2]{ 0, 0 };
		};

		static constexpr VkClearValue m_clearColorValue{ .color = {0.0f, 0.0f, 0.0f, 1.0f} };
		static constexpr VkClearValue m_clearDepthStencilValue{ .depthStencil = { 1.0f, 0 } };

		vvh::Buffer m_uniformBuffersPerFrame{};
		vvh::Buffer m_storageBuffersLights{};

		VkSampler m_sampler{ VK_NULL_HANDLE };
		VkSampler m_shadowSampler{ VK_NULL_HANDLE };
		std::vector<vvh::GBufferImage> m_gBufferAttachments{};

		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame{ VK_NULL_HANDLE };
		VkDescriptorSetLayout m_descriptorSetLayoutComposition{ VK_NULL_HANDLE };
		VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
		vvh::DescriptorSet m_descriptorSetPerFrame{};
		vvh::DescriptorSet m_descriptorSetComposition{};

		std::map<int, PipelinePerType> m_geomPipesPerType;
		vvh::Pipeline m_lightingPipeline{};

		std::vector<VkCommandPool> m_commandPools{};
		std::vector<VkCommandBuffer> m_commandBuffers{};

		glm::ivec3 m_numberLightsPerType{ 0, 0, 0 };
		bool m_lightsChanged{ true };

		// ---------------------------------------------------------------------------------

	public:
		RendererDeferredCommon(const std::string& systemName, Engine& engine, const std::string& windowName);
		virtual ~RendererDeferredCommon();

	private:
		bool OnInit(const Message& message);
		bool OnPrepareNextFrame(const Message& message);
		bool OnRecordNextFrame(const Message& message);
		bool OnObjectCreate(Message& message);
		bool OnObjectDestroy(Message& message);
		bool OnWindowSize(const Message& message);
		bool OnQuit(const Message& message);

		void CreateDeferredResources();
		void DestroyDeferredResources();
		auto getPipelineType(const ObjectHandle& handle, const vvh::VertexData& vertexData) const -> const std::string;
		auto getPipelinePerType(const std::string& type) const -> const PipelinePerType*;
		void UpdateLightStorageBuffer();

	protected:
		void CreateGeometryPipeline(const VkRenderPass* renderPass = VK_NULL_HANDLE);
		void CreateLightingPipeline(const VkRenderPass* renderPass = VK_NULL_HANDLE);
		void PrepareLightingAttachments(const VkCommandBuffer& cmdBuffer);
		void ResetLightingAttachments(const VkCommandBuffer& cmdBuffer);
		void RecordObjects(const VkCommandBuffer& cmdBuffer, const VkRenderPass* renderPass = VK_NULL_HANDLE);
		void RecordLighting(const VkCommandBuffer& cmdBuffer, const VkRenderPass* renderPass = VK_NULL_HANDLE);
		auto getAttachmentFormats() const -> const std::vector<VkFormat>;

	};

}	// namespace vve
