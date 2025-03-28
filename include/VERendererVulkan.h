#pragma once

#include <any>

namespace vve {

	//-------------------------------------------------------------------------------------------------------
	// Vulkan Renderer

    class RendererVulkan : public Renderer {

    public:

        RendererVulkan(std::string systemName, Engine& engine, std::string windowName);
        virtual ~RendererVulkan();

    protected:
		bool OnExtensions(Message message);
        bool OnInit(Message message);

        bool OnPrepareNextFrame(Message message);
        bool OnRecordNextFrame(Message message);
        bool OnRenderNextFrame(Message message);
		
		bool OnTextureCreate( Message message );
		bool OnTextureDestroy( Message message );
		bool OnMeshCreate( Message message );
		bool OnMeshDestroy( Message message );

        bool OnQuit(Message message);

        const std::vector<const char*> m_validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

		std::vector<const char*> m_instanceExtensions = {
            #ifdef __APPLE__
            "VK_MVK_macos_surface",
            // The next line is only required when using API_VERSION_1_0
            // enabledInstanceExtensions.push_back("VK_KHR_get_physical_device_properties2");
            ("VK_KHR_portability_enumeration")
            #endif
        };

        std::vector<const char*> m_deviceExtensions = {
            "VK_KHR_swapchain"
            #ifdef __APPLE__
            , "VK_KHR_portability_subset"
            #endif
        };

	    VkRenderPass m_renderPass;
        bool m_clear{true};
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;
	    vh::Pipeline m_graphicsPipeline;
	    VkDescriptorPool m_descriptorPool;    
		VkCommandPool m_commandPool;
	    std::vector<VkCommandBuffer> m_commandBuffers;
	    std::vector<VkCommandBuffer> m_commandBuffersSubmit;
        std::vector<VkSemaphore> m_imageAvailableSemaphores;
	    std::vector<vh::Semaphores> m_semaphores;
		std::vector<VkFence> m_fences;
    };
};   // namespace vve

