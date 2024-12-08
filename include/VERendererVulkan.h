#pragma once

#include <any>
#include "VHInclude.h"
#include "VERenderer.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class RendererVulkan : public Renderer<ATYPE>
    {
        using System<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:

        RendererVulkan(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name );
        virtual ~RendererVulkan();
        auto GetInstance() -> VkInstance { return m_instance; }
		auto GetPhysicalDevice() -> VkPhysicalDevice { return m_physicalDevice; }
		auto GetDevice() -> VkDevice { return m_device; }
		auto GetQueueFamilies() -> vh::QueueFamilyIndices { return m_queueFamilies; }
		auto GetGraphicsQueue() -> VkQueue { return m_graphicsQueue; }
		auto GetPresentQueue() -> VkQueue { return m_presentQueue; }
		auto GetCommandPool() -> VkCommandPool { return m_commandPool; }
		auto GetVmaAllocator() -> VmaAllocator { return m_vmaAllocator; }
		auto GetSwapChain() -> vh::SwapChain { return m_swapChain; }
		auto GetRenderPass() -> VkRenderPass { return m_renderPass; }
		auto GetDescriptorSetLayout() -> VkDescriptorSetLayout { return m_descriptorSetLayout; }
		auto GetDescriptorPool() -> VkDescriptorPool { return m_descriptorPool; }
		auto GetGraphicsPipeline() -> vh::Pipeline { return m_graphicsPipeline; }
		auto GetDepthImage() -> vh::DepthImage { return m_depthImage; }
		auto GetTexture() -> vh::Texture { return m_texture; }
		auto GetGeometry() -> vh::Geometry { return m_geometry; }

    private:
        virtual void OnInit(Message message);
        virtual void OnInit2(Message message);
        virtual void OnPrepareNextFrame(Message message);
        virtual void OnRecordNextFrame(Message message);
        virtual void OnRenderNextFrame(Message message);
        virtual void OnQuit(Message message);
        virtual void OnQuit2(Message message);

        const std::vector<const char*> m_validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        const std::vector<const char*> m_deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

		const int MAX_FRAMES_IN_FLIGHT = 2;

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
	    VkDescriptorSetLayout m_descriptorSetLayout;
	    vh::Pipeline m_graphicsPipeline;
	    vh::DepthImage m_depthImage;
	    vh::Texture m_texture;
	    vh::Geometry m_geometry;
	    vh::UniformBuffers m_uniformBuffers;
	    VkDescriptorPool m_descriptorPool;
	    std::vector<VkDescriptorSet> m_descriptorSets;
	    VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
	    vh::SyncObjects m_syncObjects;

	    uint32_t m_currentFrame = 0;
	    bool m_framebufferResized = false;


    };
};   // namespace vve

