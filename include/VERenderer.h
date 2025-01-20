#pragma once

#include <memory>
#include <any>


namespace vve
{

	class RendererVulkan;

    enum class RendererType {
        FORWARD,
        DEFERRED,
        RAYTRACING
    };

    class Renderer : public System {
        friend class Engine;

	protected:	
		struct VulkanState {
        	VkSurfaceKHR 	m_surface{VK_NULL_HANDLE};
	        WindowSDL*		m_windowSDL;
			VmaAllocator 	m_vmaAllocator;
	        VkInstance 		m_instance{VK_NULL_HANDLE};
		    VkDebugUtilsMessengerEXT m_debugMessenger;
		    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
		    VkDevice 		m_device{VK_NULL_HANDLE};
		    vh::QueueFamilyIndices m_queueFamilies;
		    VkQueue 		m_graphicsQueue{VK_NULL_HANDLE};
		    VkQueue 		m_presentQueue{VK_NULL_HANDLE};
		    vh::SwapChain 	m_swapChain;
			vh::DepthImage 	m_depthImage;
			std::vector<VkCommandBuffer> m_commandBuffersSubmit;

	    	uint32_t m_currentFrame = MAX_FRAMES_IN_FLIGHT - 1;
        	uint32_t m_imageIndex;
	    	bool m_framebufferResized = false;
		};

		using VulkanStateHandle = vsty::strong_type_t<vecs::Handle, vsty::counter<>>;

    public:
        Renderer(std::string systemName, Engine& m_engine, std::string windowName);
        virtual ~Renderer();
        
    protected:
		auto GetSurface() -> VkSurfaceKHR&;
		auto GetInstance() -> VkInstance&;
		auto GetPhysicalDevice() -> VkPhysicalDevice&;
		auto GetDevice() -> VkDevice&;
		auto GetQueueFamilies() -> vh::QueueFamilyIndices&;
		auto GetGraphicsQueue() -> VkQueue&;
		auto GetPresentQueue() -> VkQueue&;
		auto GetCommandPool() -> VkCommandPool&;
		auto GetVmaAllocator() -> VmaAllocator&;
		auto GetSwapChain() -> vh::SwapChain&;
		auto GetDepthImage() -> vh::DepthImage&;
		auto GetCurrentFrame() -> uint32_t&;
		auto GetImageIndex() -> uint32_t&;
		auto GetFramebufferResized() -> bool&;

		auto GetVulkanState() -> VulkanState&;
		void SubmitCommandBuffer( VkCommandBuffer commandBuffer );

		bool OnAnnounce(Message message);
		std::string 	m_windowName;
        Window* 		m_window;
		//RendererVulkan* m_vulkan;

		VulkanStateHandle m_vulkanStateHandle{};
    };

};   // namespace vve