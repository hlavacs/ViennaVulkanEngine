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
		auto GetCurrentFrame() -> uint32_t;
		auto GetImageIndex() -> uint32_t;
		void SubmitCommandBuffer( VkCommandBuffer commandBuffer );

    protected:
		void OnAnnounce(Message message);
        VkSurfaceKHR	m_surface{VK_NULL_HANDLE};
		std::string 	m_windowName;
        Window* 		m_window;
		RendererVulkan* m_vulkan;
    };

};   // namespace vve