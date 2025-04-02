#pragma once

namespace vve {

	class RendererDeferred11 : public Renderer {

		friend class RendererDeferred;

	public:
		RendererDeferred11(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferred11();

	private:
		bool OnInit(Message message);
		bool OnQuit(Message message);
		void CreatePipelines();

		vh::UniformBuffers m_uniformBuffersPerFrame{};
		// TODO: might need a SSBO for many lights
		vh::UniformBuffers m_uniformBuffersLights{};

		// TODO: Maybe make GBufferAttachment struct for better alignment
		VkSampler m_sampler{ VK_NULL_HANDLE };
		vh::GBufferImage m_positionImage{};
		vh::GBufferImage m_normalsImage{};
		vh::GBufferImage m_albedoImage{};

		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame{ VK_NULL_HANDLE };
		VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
		vh::DescriptorSet m_descriptorSetPerFrame{};

		VkRenderPass m_geometryPass{ VK_NULL_HANDLE };
		VkRenderPass m_lightingPass{ VK_NULL_HANDLE };

		VkCommandPool m_commandPool{ VK_NULL_HANDLE };
		std::vector<VkCommandBuffer> m_commandBuffers;

		// TODO: maybe constexpr is not a good idea? -> also increase number later!!!
		static constexpr size_t m_maxNumberLights{ 16 };
	};

}	// namespace vve
