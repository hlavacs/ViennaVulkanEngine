#pragma once

#include <memory>
#include <any>


namespace vve {

	class RendererVulkan;

    enum class RendererType {
        FORWARD,
        DEFERRED,
        RAYTRACING
    };

	struct VulkanState {
		uint32_t 		m_apiVersionInstance{VK_API_VERSION_1_1};
		uint32_t 		m_apiVersionDevice{VK_API_VERSION_1_1};
		VkInstance 		m_instance{VK_NULL_HANDLE};
		VkSurfaceKHR 	m_surface{VK_NULL_HANDLE};
		VmaAllocator 	m_vmaAllocator;
		VkDebugUtilsMessengerEXT m_debugMessenger;
		
		VkPhysicalDevice 			m_physicalDevice{VK_NULL_HANDLE};
		VkPhysicalDeviceFeatures 	m_physicalDeviceFeatures;
		VkPhysicalDeviceProperties 	m_physicalDeviceProperties;

		VkDevice 		m_device{VK_NULL_HANDLE};
		vh::QueueFamilyIndices m_queueFamilies;
		VkQueue 		m_graphicsQueue{VK_NULL_HANDLE};
		VkQueue 		m_presentQueue{VK_NULL_HANDLE};
		vh::SwapChain 	m_swapChain;
		vh::DepthImage 	m_depthImage;
		VkFormat		m_depthMapFormat{VK_FORMAT_UNDEFINED};
		VkCommandPool 	m_commandPool{VK_NULL_HANDLE};
		std::vector<VkCommandBuffer> m_commandBuffersSubmit;

		uint32_t m_currentFrame = MAX_FRAMES_IN_FLIGHT - 1;
		uint32_t m_imageIndex;
		bool m_framebufferResized = false;
	};

    class Renderer : public System {
        friend class Engine;

    public:
        Renderer(std::string systemName, Engine& m_engine, std::string windowName);
        virtual ~Renderer();
		static auto GetState(vecs::Registry& registry) -> std::tuple< vecs::Handle, vecs::Ref<VulkanState>>;

    protected:
		bool OnInit(Message message);
		void SubmitCommandBuffer( VkCommandBuffer commandBuffer );

		std::string 				m_windowName;
		vecs::Ref<WindowState> 		m_windowState{};
		vecs::Ref<WindowSDLState> 	m_windowSDLState{};
		vecs::Handle 				m_vulkanStateHandle{};
		vecs::Ref<VulkanState> 		m_vulkanState{};
    };

};   // namespace vve