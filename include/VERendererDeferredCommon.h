#pragma once

namespace vve {

	template <typename Derived>
	class RendererDeferredCommon : public Renderer{

	protected:
		static constexpr uint32_t MAX_NUMBER_LIGHTS{ 128 };

		enum GBufferIndex { POSITION = 0, NORMAL = 1, ALBEDO = 2, DEPTH = 3 };

		struct PipelinePerType {
			std::string m_type;
			VkDescriptorSetLayout m_descriptorSetLayoutPerObject{};
			vvh::Pipeline m_graphicsPipeline{};
		};

		struct PushConstantsLight {
			alignas(16) glm::mat4 invViewProj;
			vvh::LightOffset offset;
		};

		static constexpr std::array<VkClearValue, 4> m_clearValues = { {
			VkClearValue{.color = {{ 0.0f, 0.0f, 0.0f, 1.0f }} },
			VkClearValue{.color = {{ 0.0f, 0.0f, 0.0f, 1.0f }} },
			VkClearValue{.color = {{ 0.0f, 0.0f, 0.0f, 1.0f }} },
			VkClearValue{.depthStencil = { 1.0f, 0 } }
		} };

		vvh::Buffer m_uniformBuffersPerFrame{};
		vvh::Buffer m_storageBuffersLights{};

		VkSampler m_sampler{ VK_NULL_HANDLE };
		std::array<vvh::GBufferImage, 3> m_gBufferAttachments{};

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

		// ---------------------------------------------------------------------------------

	public:
		RendererDeferredCommon(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferredCommon();

	private:
		bool OnInit(Message message);
		bool OnPrepareNextFrame(Message message);
		bool OnRecordNextFrame(Message message);
		bool OnObjectCreate(Message message);
		bool OnObjectDestroy(Message message);
		bool OnWindowSize(Message message);
		bool OnQuit(Message message);

		void CreateDeferredResources();
		void DestroyDeferredResources();
		auto getPipelineType(ObjectHandle handle, vvh::VertexData& vertexData) -> std::string;
		auto getPipelinePerType(std::string type) -> PipelinePerType*;
		auto getAttachmentFormats() -> std::vector<VkFormat>;

	protected:
		void CreateGeometryPipeline(const VkRenderPass& renderPass = VK_NULL_HANDLE);
		void CreateLightingPipeline(const VkRenderPass& renderPass = VK_NULL_HANDLE);
		void PrepareLightingAttachments(VkCommandBuffer& cmdBuffer);
		void ResetLightingAttachments(VkCommandBuffer& cmdBuffer);
		void RecordObjects(VkCommandBuffer& cmdBuffer, VkRenderPass* renderPass = VK_NULL_HANDLE);
		void RecordLighting(VkCommandBuffer& cmdBuffer, VkRenderPass* renderPass = VK_NULL_HANDLE);

	};

}	// namespace vve
