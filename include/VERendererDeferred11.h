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

		vh::UniformBuffers m_uniformBuffersPerFrame{};
		// TODO: might need a SSBO for many lights
		vh::UniformBuffers m_uniformBuffersLights{};
		vh::Map m_texturePosition{};

		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame{ VK_NULL_HANDLE };
		VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
		vh::DescriptorSet m_descriptorSetPerFrame{};

		VkRenderPass m_renderPass{ VK_NULL_HANDLE };

		VkCommandPool m_commandPool{ VK_NULL_HANDLE };
		std::vector<VkCommandBuffer> m_commandBuffers;

		// TODO: maybe constexpr is not a good idea? -> also increase number later!!!
		static constexpr size_t m_maxNumberLights{ 16 };
	};

}	// namespace vve
