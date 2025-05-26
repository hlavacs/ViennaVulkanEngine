#pragma once

namespace vve {

	template <typename Derived>
	class RendererDeferredCommon : public Renderer{

	protected:

		enum GBufferIndex { POSITION = 0, NORMAL = 1, ALBEDO = 2, DEPTH = 3 };

		struct PipelinePerType {
			std::string m_type;
			VkDescriptorSetLayout m_descriptorSetLayoutPerObject{};
			vvh::Pipeline m_graphicsPipeline{};
		};

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
		int m_pass{ 0 };

		// ---------------------------------------------------------------------------------

	public:
		RendererDeferredCommon(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferredCommon();

	private:

		bool OnInit(Message message);
		bool OnPrepareNextFrame(Message message);
		bool OnObjectCreate(Message message);
		bool OnObjectDestroy(Message message);

		void CreateDeferredResources();
		auto getPipelineType(ObjectHandle handle, vvh::VertexData& vertexData) -> std::string;
		auto getPipelinePerType(std::string type) -> PipelinePerType*;

	};

}	// namespace vve
