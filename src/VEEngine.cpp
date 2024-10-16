#define VOLK_IMPLEMENTATION
#include "VEEngine.h"
#include "VEWindowSDL.h"
//#include "VHImgui.h"

namespace vve {

	VeEngine::VeEngine() {
	#ifndef NDEBUG
		m_debug = true;
	#endif
		Init();
	};
	
	VeEngine::~VeEngine() {
		m_renderer = nullptr;
		m_sceneManager = nullptr;
		m_window = nullptr;
		Shutdown();
	};
	
	void VeEngine::Init(){
		if(m_initialized) return;
		CreateWindow("Vulkan Engine", 800, 600);
		SetupVulkan();
		CreateRenderer("Forward");
		CreateCamera("Main Camera");
		CreateSceneManager("");
		LoadLevel("");
		m_initialized = true;
	};
	
	void VeEngine::SetupVulkan() {

		if(m_debug) {
	        m_instance_layers.push_back("VK_LAYER_KHRONOS_validation");
	        //m_instance_layers.push_back("VK_LAYER_KHRONOS_");
	        m_instance_extensions.push_back("VK_EXT_debug_report");
		}
	
		//VkResult volkInitialize();
		vh::SetUpInstance(m_instance_layers, m_instance_extensions, m_allocator, &m_instance);
		//volkLoadInstance(m_instance);

		if(m_debug) vh::SetupDebugReport(m_instance, m_allocator, &m_debugReport);
		vh::SetupPhysicalDevice(m_instance, m_device_extensions, &m_physicalDevice);
		vh::SetupGraphicsQueueFamily(m_physicalDevice, &m_queueFamily);
	    vh::SetupDevice( m_physicalDevice, nullptr, m_device_extensions, m_queueFamily, &m_device);
		//volkLoadDevice(m_device);

		vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
		
		m_window->Init();
	};
	
	
	void VeEngine::RegisterSystem( std::shared_ptr<VeSystem> system) {
		// Register system
	}
	
	
	void VeEngine::LoadLevel( const char* levelName ){
		// Load level
	};
	
	void VeEngine::CreateWindow( const char* windowName, int width, int height ){
		m_window = std::make_unique<VeWindowSDL>(*this, m_instance, windowName, width, height, m_instance_extensions);
	};
	
	void VeEngine::CreateRenderer( const char* rendererName){
		// Create renderer
	};
	
	void VeEngine::CreateCamera( const char* cameraName ){
		// Create camera
	};
	
	void VeEngine::CreateSceneManager( const char* sceneManagerName ){
		// Create scene manager
	};
	
	void VeEngine::Run(){
		Init();
		m_running = true;
		while(m_running) { //call stop to stop the engine
			// Window
			// Update
			// Render
			// GUI

			m_window->pollEvents();
			m_window->renderNextFrame();
		}
	};
	
	void VeEngine::Stop(){
		m_running = false;
	};
	
	
	void VeEngine::Shutdown(){
	    auto PFN_DestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
	    PFN_DestroyDebugReportCallbackEXT(m_instance, m_debugReport, m_allocator);
	
	    vkDestroyDevice(m_device, m_allocator);
	    vkDestroyInstance(m_instance, m_allocator);
	}

};   // namespace vve

