

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

        engine->RegisterSystem( { 
			  {this, -1000, MessageType::INIT, [this](Message message){this->OnInit(message);} }
			, {this, -1000, MessageType::PREPARE_NEXT_FRAME, [this](Message message){this->OnPrepareNextFrame(message);} }
			, {this, -1000, MessageType::RECORD_NEXT_FRAME, [this](Message message){this->OnRecordNextFrame(message);} }
			, {this, -1000, MessageType::RENDER_NEXT_FRAME, [this](Message message){this->OnRenderNextFrame(message);} }
			, {this,    50, MessageType::INIT, [this](Message message){this->OnInit2(message);} }
			, {this,  1000, MessageType::QUIT, [this](Message message){this->OnQuit(message);} }
		} );
    }

    template<ArchitectureType ATYPE>
    RendererVulkan<ATYPE>::~RendererVulkan() {}


    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnInit(Message message) {
        WindowSDL<ATYPE>* window = (WindowSDL<ATYPE>*)m_window;

        m_instance_extensions = window->GetInstanceExtensions();
        if(m_engine->GetDebug()) {
   	        m_instance_layers.push_back("VK_LAYER_KHRONOS_validation");
   	        m_instance_extensions.push_back("VK_EXT_debug_report");
   		}
   		//VkResult volkInitialize();
   		vh::SetupInstance(m_instance_layers, m_instance_extensions, m_allocator, &m_instance);
   		//volkLoadInstance(m_instance);       
   		if(m_engine->GetDebug()) vh::SetupDebugReport(m_instance, m_allocator, &m_debugReport);
    }


    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnInit2(Message message) {
        WindowSDL<ATYPE>* window = (WindowSDL<ATYPE>*)(m_engine->GetSystem("VVE WindowSDL"));

   		vh::SetupPhysicalDevice(m_instance, m_device_extensions, &m_physicalDevice);
   		vh::SetupGraphicsQueueFamily(m_physicalDevice, &m_queueFamily);
   	    vh::SetupDevice( m_physicalDevice, nullptr, m_device_extensions, m_queueFamily, &m_device);
   		//volkLoadDevice(m_device);
   		vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);

        //-------------------------------------------------------------------------

        /*m_mainWindowData.Surface = window->GetSurface();
        // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(rend->GetPhysicalDevice(), rend->GetQueueFamily(), m_mainWindowData.Surface, &res);
        if (res != VK_TRUE) {
            fprintf(stderr, "Error no WSI support on physical device 0\n");
            exit(-1);
        }
        // Select Surface Format
        std::vector<VkFormat> requestSurfaceFormats = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
        m_mainWindowData.SurfaceFormat = vh::SelectSurfaceFormat(rend->GetPhysicalDevice(), m_mainWindowData.Surface, requestSurfaceFormats);
        // Select Present Mode
        std::vector<VkPresentModeKHR> requestedPresentModes = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR };
        m_mainWindowData.PresentMode = vh::SelectPresentMode(rend->GetPhysicalDevice(), m_mainWindowData.Surface, requestedPresentModes);
        auto width = window->GetWidth();
        auto height = window->GetHeight();
        vh::CreateWindowSwapChain(rend->GetPhysicalDevice(), rend->GetDevice(), &m_mainWindowData, rend->GetAllocator(), width, height, window->GetMinImageCount());
        vh::CreateWindowCommandBuffers(rend->GetPhysicalDevice(), rend->GetDevice(), &m_mainWindowData, rend->GetQueueFamily(), rend->GetAllocator());
        vh::CreateDescriptorPool(rend->GetDevice(), &m_descriptorPool);
        */


    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnPrepareNextFrame(Message message) {

    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnRecordNextFrame(Message message) {

    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnRenderNextFrame(Message message) {

    }
    
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