#pragma once

#include <any>

namespace vve {

	//-------------------------------------------------------------------------------------------------------
	// Vulkan Renderer

   	template<ArchitectureType ATYPE>
    class RendererVulkan : public Renderer<ATYPE>
    {
        using System<ATYPE>::m_engine;
        using System<ATYPE>::m_registry;
		using typename System<ATYPE>::Message;
		using typename System<ATYPE>::MsgExtensions;
		using typename System<ATYPE>::MsgAnnounce;
		using typename System<ATYPE>::MsgObjectCreate;
		using typename System<ATYPE>::MsgTextureCreate;
		using typename System<ATYPE>::MsgTextureDestroy;
		using typename System<ATYPE>::MsgGeometryCreate;
		using typename System<ATYPE>::MsgGeometryDestroy;
        using Renderer<ATYPE>::m_window;

    public:

        RendererVulkan(std::string systemName, Engine<ATYPE>& engine);
        virtual ~RendererVulkan();
        auto GetInstance() -> VkInstance { return m_instance; }
		auto GetPhysicalDevice() -> VkPhysicalDevice { return m_physicalDevice; }
		auto GetDevice() -> VkDevice { return m_device; }
		auto GetQueueFamilies() -> vh::QueueFamilyIndices { return m_queueFamilies; }
		auto GetGraphicsQueue() -> VkQueue { return m_graphicsQueue; }
		auto GetPresentQueue() -> VkQueue { return m_presentQueue; }
		auto GetCommandPool() -> VkCommandPool { return m_commandPool; }
		auto GetVmaAllocator() -> VmaAllocator { return m_vmaAllocator; }
		auto GetSwapChain() -> vh::SwapChain& { return m_swapChain; }
		auto GetRenderPass() -> VkRenderPass { return m_renderPass; }
		auto GetDescriptorPool() -> VkDescriptorPool { return m_descriptorPool; }
		auto GetGraphicsPipeline() -> vh::Pipeline& { return m_graphicsPipeline; }
		auto GetDepthImage() -> vh::DepthImage& { return m_depthImage; }

		auto GetCommandBuffers() -> std::vector<VkCommandBuffer>& { return m_commandBuffers; }
		auto GetSyncObjects() -> std::vector<vh::Semaphores>& { return m_semaphores; }
		auto GetCurrentFrame() -> uint32_t { return m_currentFrame; }
		auto GetImageIndex() -> uint32_t { return m_imageIndex; }
		auto GetFramebufferResized() -> bool { return m_framebufferResized; }

		void SubmitCommandBuffer( VkCommandBuffer commandBuffer ) { m_commandBuffersSubmit.push_back(commandBuffer); };

    private:
		void OnExtensions(Message message);
        void OnInit(Message message);
        void OnInit2(Message message);

        void OnPrepareNextFrame(Message message);
        void OnRecordNextFrame(Message message);
        void OnRenderNextFrame(Message message);
		
		void OnObjectCreate( Message message );
		void OnTextureCreate( Message message );
		void OnTextureDestroy( Message message );
		void OnGeometryCreate( Message message );
		void OnGeometryDestroy( Message message );

        void OnQuit(Message message);
        void OnQuit2(Message message);

        const std::vector<const char*> m_validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

		std::vector<const char*> m_instanceExtensions;

        std::vector<const char*> m_deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        WindowSDL<ATYPE> *m_windowSDL;
		VmaAllocator m_vmaAllocator;
        VkInstance m_instance;
	    VkDebugUtilsMessengerEXT m_debugMessenger;
	    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	    VkDevice m_device;
	    vh::QueueFamilyIndices m_queueFamilies;
	    VkQueue m_graphicsQueue;
	    VkQueue m_presentQueue;
	    vh::SwapChain m_swapChain;
	    VkRenderPass m_renderPass;

		vh::DescriptorSetLayouts m_descriptorSetLayouts;
	    vh::Pipeline m_graphicsPipeline;
	    vh::DepthImage m_depthImage;

	    VkDescriptorPool m_descriptorPool;
	    
		VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
	    std::vector<VkCommandBuffer> m_commandBuffersSubmit;
		
        std::vector<VkSemaphore> m_imageAvailableSemaphores;
	    std::vector<vh::Semaphores> m_semaphores;
		std::vector<VkFence> m_fences;

	    uint32_t m_currentFrame = MAX_FRAMES_IN_FLIGHT - 1;
        uint32_t m_imageIndex;

	    bool m_framebufferResized = false;


    };
};   // namespace vve

