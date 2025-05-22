#pragma once

namespace vve {

	class RendererDeferred13 : public Renderer {

		//struct PipelinePerType {
		//	std::string m_type;
		//	VkDescriptorSetLayout m_descriptorSetLayoutPerObject{};
		//	vvh::Pipeline m_graphicsPipeline{};
		//};

	public:
		static constexpr uint32_t MAX_NUMBER_LIGHTS{ 128 };

		RendererDeferred13(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferred13();

	private:
		bool OnInit(Message message);
		//bool OnPrepareNextFrame(Message message);
		//bool OnRecordNextFrame(Message message);
		//bool OnObjectCreate(Message message);
		//bool OnObjectDestroy(Message message);
		//bool OnWindowSize(Message message);
		bool OnQuit(Message message);
		//void CreateGeometryPipeline();
		//void CreateLightingPipeline();

		//void CreateDeferredResources();
		//void DestroyDeferredResources();
		//auto getPipelinePerType(std::string type) -> PipelinePerType*;
		//auto getPipelineType(ObjectHandle handle, vvh::VertexData& vertexData) -> std::string;

		//std::vector<VkFramebuffer> m_gBufferFrameBuffers{};
		//std::vector<VkFramebuffer> m_lightingFrameBuffers{};

		vvh::Buffer m_uniformBuffersPerFrame{};
		vvh::Buffer m_storageBuffersLights{};

		VkSampler m_sampler{ VK_NULL_HANDLE };
		std::array<vvh::GBufferImage, 3> m_gBufferAttachments{};

		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame{ VK_NULL_HANDLE };
		VkDescriptorSetLayout m_descriptorSetLayoutComposition{ VK_NULL_HANDLE };
		VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
		vvh::DescriptorSet m_descriptorSetPerFrame{};
		vvh::DescriptorSet m_descriptorSetComposition{};

		//VkRenderPass m_geometryPass{ VK_NULL_HANDLE };
		//VkRenderPass m_lightingPass{ VK_NULL_HANDLE };

		//std::map<int, PipelinePerType> m_geomPipesPerType;
		//vvh::Pipeline m_lightingPipeline{};

		std::vector<VkCommandPool> m_commandPools{};
		//std::vector<VkCommandBuffer> m_commandBuffers{};

		//glm::ivec3 m_numberLightsPerType{ 0, 0, 0 };
		//int m_pass{ 0 };
	};

}	// namespace vve
