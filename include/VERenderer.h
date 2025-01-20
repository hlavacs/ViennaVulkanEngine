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
	        VkInstance 		m_instance;
		    VkDebugUtilsMessengerEXT m_debugMessenger;
		    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
		    VkDevice 		m_device;
		    vh::QueueFamilyIndices m_queueFamilies;
		    VkQueue 		m_graphicsQueue;
		    VkQueue 		m_presentQueue;
		    vh::SwapChain 	m_swapChain;
			
	    	uint32_t m_currentFrame = MAX_FRAMES_IN_FLIGHT - 1;
        	uint32_t m_imageIndex;
	    	bool m_framebufferResized = false;
		};

    public:
        Renderer(std::string systemName, Engine& m_engine, std::string windowName);
        virtual ~Renderer();
        
		auto GetSurface() -> VkSurfaceKHR;
		auto GetInstance() -> VkInstance;
		auto GetPhysicalDevice() -> VkPhysicalDevice;
		auto GetDevice() -> VkDevice;
		auto GetQueueFamilies() -> vh::QueueFamilyIndices;
		auto GetGraphicsQueue() -> VkQueue;
		auto GetPresentQueue() -> VkQueue;
		auto GetCommandPool() -> VkCommandPool;
		auto GetDescriptorPool() -> VkDescriptorPool;
		auto GetVmaAllocator() -> VmaAllocator&;
		auto GetSwapChain() -> vh::SwapChain&;
		auto GetDepthImage() -> vh::DepthImage&;
		auto GetCurrentFrame() -> uint32_t&;
		auto GetImageIndex() -> uint32_t&;
		void SubmitCommandBuffer( VkCommandBuffer commandBuffer );

    protected:
		bool OnAnnounce(Message message);
		std::string 	m_windowName;
        Window* 		m_window;
		RendererVulkan* m_vulkan;

		VulkanState* m_vulkanStatePtr{};
    };

};   // namespace vve