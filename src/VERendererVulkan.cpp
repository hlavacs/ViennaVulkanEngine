

#include "VHInclude.h"
#include "VERendererVulkan.h"
#include "VESystem.h"
#include "VEInclude.h"
#include "VHInclude.h"
#include "VEEngine.h"
#include "VEWindowSDL.h"


namespace vve {

    template<ArchitectureType ATYPE>
    RendererVulkan<ATYPE>::RendererVulkan(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name ) 
        : Renderer<ATYPE>(engine, window, name) {

        engine->RegisterSystem( this,  -1000
            , {MessageType::INIT, MessageType::PREPARE_NEXT_FRAME
                , MessageType::RECORD_NEXT_FRAME, MessageType::RENDER_NEXT_FRAME} );

        engine->RegisterSystem( this, 1000, {MessageType::QUIT} );
    }

    template<ArchitectureType ATYPE>
    RendererVulkan<ATYPE>::~RendererVulkan() {}

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnInit(Message message) {
        WindowSDL<ATYPE>* window = (WindowSDL<ATYPE>*)(m_engine->GetSystem("VVE WindowSDL"));

        m_instance_extensions = window->GetInstanceExtensions();
        if(m_engine->GetDebug()) {
	        m_instance_layers.push_back("VK_LAYER_KHRONOS_validation");
	        m_instance_extensions.push_back("VK_EXT_debug_report");
		}
	
		//VkResult volkInitialize();
		vh::SetUpInstance(m_instance_layers, m_instance_extensions, m_allocator, &m_instance);
		//volkLoadInstance(m_instance);

		if(m_engine->GetDebug()) vh::SetupDebugReport(m_instance, m_allocator, &m_debugReport);
		vh::SetupPhysicalDevice(m_instance, m_device_extensions, &m_physicalDevice);
		vh::SetupGraphicsQueueFamily(m_physicalDevice, &m_queueFamily);
	    vh::SetupDevice( m_physicalDevice, nullptr, m_device_extensions, m_queueFamily, &m_device);
		//volkLoadDevice(m_device);

		vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnPrepareNextFrame(Message message) {}

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnRenderNextFrame(Message message) {}
    
    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnQuit(Message message) { 
	    auto PFN_DestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
	    PFN_DestroyDebugReportCallbackEXT(m_instance, m_debugReport, m_allocator);
	
	    vkDestroyDevice(m_device, m_allocator);
	    vkDestroyInstance(m_instance, m_allocator);
    }

    template class RendererVulkan<ArchitectureType::SEQUENTIAL>;
    template class RendererVulkan<ArchitectureType::PARALLEL>;

};   // namespace vve