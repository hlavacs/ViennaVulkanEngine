#pragma once

namespace vve {

	class RendererDeferred11 : public Renderer {

		friend class RendererDeferred;

		enum GBufferIndex { POSITION = 0, NORMAL = 1, ALBEDO = 2 };

		struct PipelinePerType {
			std::string m_type;
			VkDescriptorSetLayout m_descriptorSetLayoutPerObject{};
			vvh::Pipeline m_graphicsPipeline{};
		};

	public:
		static constexpr uint32_t MAX_NUMBER_LIGHTS{ 128 };

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

		void getBindingDescription(std::string type, std::string C, int& binding, int stride, auto& bdesc);
		auto getBindingDescriptions(std::string type) -> std::vector<VkVertexInputBindingDescription>;
		void getAttributeDescription(std::string type, std::string C, int& binding, int& location, VkFormat format, auto& attd);
		auto getAttributeDescriptions(std::string type) -> std::vector<VkVertexInputAttributeDescription>;
		template<typename T>
		auto RegisterLight(float type, std::vector<vvh::Light>& lights, int& i) -> int;

		auto getPipelinePerType(std::string type) -> PipelinePerType*;
		auto getPipelineType(ObjectHandle handle, vvh::VertexData& vertexData) -> std::string;

		std::vector<VkFramebuffer> m_gBufferFrameBuffers{};

		vvh::Buffer m_uniformBuffersPerFrame{};	// TODO: maybe rename
		vvh::Buffer m_storageBuffersLights{};

		// TODO: Maybe make GBufferAttachment struct for better alignment
		VkSampler m_sampler{ VK_NULL_HANDLE };
		std::array<vvh::GBufferImage, 3> m_gBufferAttachments{};

		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame{ VK_NULL_HANDLE };
		VkDescriptorSetLayout m_descriptorSetLayoutComposition{ VK_NULL_HANDLE };
		VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
		vvh::DescriptorSet m_descriptorSetPerFrame{};
		vvh::DescriptorSet m_descriptorSetComposition{};

		VkRenderPass m_geometryPass{ VK_NULL_HANDLE };
		VkRenderPass m_lightingPass{ VK_NULL_HANDLE };

		std::map<int, PipelinePerType> m_geomPipesPerType;
		vvh::Pipeline m_lightingPipeline{};

		std::vector<VkCommandPool> m_commandPools{};
		std::vector<VkCommandBuffer> m_commandBuffers{};

		glm::ivec3 m_numberLightsPerType{ 0,0,0 };
		int m_pass{ 0 };
	};

}	// namespace vve
