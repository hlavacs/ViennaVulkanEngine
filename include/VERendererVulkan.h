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

		std::vector<const char*> m_instanceExtensions;

        std::vector<const char*> m_deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

	    VkRenderPass m_renderPass;
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

