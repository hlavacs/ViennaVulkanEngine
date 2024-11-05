#pragma once

#include <any>
#include "VERenderer.h"

namespace vve {

   	template<ArchitectureType ATYPE>
    class RendererVulkan : public Renderer<ATYPE>
    {
        using System<ATYPE>::m_engine;
        using Renderer<ATYPE>::m_window;

    public:

        RendererVulkan(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name = "VVE RendererVulkan" );
        virtual ~RendererVulkan();

        auto GetAllocator() -> VkAllocationCallbacks* { return m_allocator; };
        auto GetInstance() -> VkInstance { return m_instance; };
        auto GetDebugReport() -> VkDebugReportCallbackEXT { return m_debugReport; };
        auto GetPhysicalDevice() -> VkPhysicalDevice { return m_physicalDevice; };
        auto GetDevice() -> VkDevice { return m_device; };
        auto GetQueueFamily() -> uint32_t { return m_queueFamily; };
        auto GetQueue() -> VkQueue { return m_queue; };
        auto GetPipelineCache() -> VkPipelineCache { return m_pipelineCache; };
        auto GetInstanceLayers() -> std::vector<const char*> { return m_instance_layers; };
        auto GetInstanceExtensions() -> std::vector<const char*> { return m_instance_extensions; };
        auto GetDeviceExtensions() -> std::vector<const char*> { return m_device_extensions; };

    private:
        virtual void OnInit(Message message);
        virtual void OnPrepareNextFrame(Message message);
        virtual void OnRecordNextFrame(Message message);
        virtual void OnRenderNextFrame(Message message);
        virtual void OnQuit(Message message);

   		VkDescriptorPool         m_descriptorPool = VK_NULL_HANDLE;
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

};   // namespace vve

