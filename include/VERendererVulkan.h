#pragma once

#include <any>

namespace vve {

	//-------------------------------------------------------------------------------------------------------
	// Vulkan Renderer

    class RendererVulkan : public Renderer {

    public:

        RendererVulkan(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererVulkan();
		
		auto GetSurface() -> VkSurfaceKHR { return  m_vulkanState.m_surface; };
        auto GetInstance() -> VkInstance { return m_vulkanState.m_instance; }
		auto GetPhysicalDevice() -> VkPhysicalDevice { return m_vulkanState.m_physicalDevice; }
		auto GetDevice() -> VkDevice { return m_vulkanState.m_device; }
		auto GetQueueFamilies() -> vh::QueueFamilyIndices { return m_vulkanState.m_queueFamilies; }
		auto GetGraphicsQueue() -> VkQueue { return m_vulkanState.m_graphicsQueue; }
		auto GetPresentQueue() -> VkQueue { return m_vulkanState.m_presentQueue; }
		auto GetCommandPool() -> VkCommandPool { return m_commandPool; }
		auto GetDescriptorPool() -> VkDescriptorPool { return m_descriptorPool; }
		auto GetVmaAllocator() -> VmaAllocator& { return m_vulkanState.m_vmaAllocator; }
		auto GetSwapChain() -> vh::SwapChain& { return m_vulkanState.m_swapChain; }
		auto GetDepthImage() -> vh::DepthImage& { return m_depthImage; }
		auto GetCurrentFrame() -> uint32_t& { return m_vulkanState.m_currentFrame; }
		auto GetImageIndex() -> uint32_t& { return m_vulkanState.m_imageIndex; }
		auto GetFramebufferResized() -> bool& { return m_vulkanState.m_framebufferResized; }

		auto GetVulkanState() -> VulkanState& { return m_vulkanState; }

		auto GetRenderPass() -> VkRenderPass { return m_renderPass; }
		void SubmitCommandBuffer( VkCommandBuffer commandBuffer );

    protected:
		bool OnExtensions(Message message);
        bool OnInit(Message message);

        bool OnPrepareNextFrame(Message message);
        bool OnRecordNextFrame(Message message);
        bool OnRenderNextFrame(Message message);
		
		bool OnTextureCreate( Message message );
		bool OnTextureDestroy( Message message );
		bool OnGeometryCreate( Message message );
		bool OnGeometryDestroy( Message message );

        bool OnQuit(Message message);

        const std::vector<const char*> m_validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

		std::vector<const char*> m_instanceExtensions;

        std::vector<const char*> m_deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

		VulkanState m_vulkanState{};

	    VkRenderPass m_renderPass;
		VkDescriptorSetLayout m_descriptorSetLayoutPerObject;
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
	    vh::Pipeline m_graphicsPipeline;
	    vh::DepthImage m_depthImage;
	    VkDescriptorPool m_descriptorPool;    
		VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
	    std::vector<VkCommandBuffer> m_commandBuffersSubmit;
        std::vector<VkSemaphore> m_imageAvailableSemaphores;
	    std::vector<vh::Semaphores> m_semaphores;
		std::vector<VkFence> m_fences;
    };
};   // namespace vve

