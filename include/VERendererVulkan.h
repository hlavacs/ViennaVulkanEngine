#pragma once

#include <any>
#include "VERenderer.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class Window;

    struct VulkanState {
		VkAllocationCallbacks*   m_allocator = nullptr;
		VkInstance               m_instance = VK_NULL_HANDLE;
		VkDebugReportCallbackEXT m_debugReport = VK_NULL_HANDLE;
		VkPhysicalDevice         m_physicalDevice = VK_NULL_HANDLE;
		VkDevice                 m_device = VK_NULL_HANDLE;
		uint32_t                 m_queueFamily = (uint32_t)-1;		
		VkQueue                  m_queue = VK_NULL_HANDLE;
		VkPipelineCache          m_pipelineCache = VK_NULL_HANDLE;
		std::vector<const char*> m_instance_layers;
		std::vector<const char*> m_instance_extensions;
		std::vector<const char*> m_device_extensions{"VK_KHR_swapchain"};
	};


   	template<ArchitectureType ATYPE>
    class RendererVulkan : public Renderer<ATYPE>
    {
        using System<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:

        RendererVulkan(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name = "VVE RendererVulkan" );
        virtual ~RendererVulkan();
        auto GetState() -> const VulkanState*  { return &m_state; };

    private:
        virtual void OnInit(Message message) override;
        virtual void OnPrepareNextFrame(Message message) override;
        virtual void OnRenderNextFrame(Message message) override;
        virtual void OnQuit(Message message) override;

   		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        VulkanState m_state;
    };

};   // namespace vve

