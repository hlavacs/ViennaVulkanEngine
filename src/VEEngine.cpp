#define VOLK_IMPLEMENTATION
#include "VEEngine.h"
#include "VEWindowSDL.h"

namespace vve {

	Engine::Engine() {
	#ifndef NDEBUG
		m_debug = true;
	#endif
		Init();
	};
	
	Engine::~Engine() {
		Shutdown();
	};
	
	void Engine::Init(){
		if(m_initialized) return;
		CreateWindow("Vulkan Engine", 800, 600);
		SetupVulkan();
		CreateRenderer("Forward");
		CreateCamera("Main Camera");
		CreateSceneManager("");
		LoadLevel("");
		m_initialized = true;
	};
	
	void Engine::SetupVulkan() {

		if(m_debug) {
	        m_instance_layers.push_back("VK_LAYER_KHRONOS_validation");
	        m_instance_extensions.push_back("VK_EXT_debug_report");
		}
	
		//VkResult volkInitialize();
		vh::SetUpInstance(m_instance_layers, m_instance_extensions, m_state.m_allocator, &m_state.m_instance);
		//volkLoadInstance(m_instance);

		if(m_debug) vh::SetupDebugReport(m_state.m_instance, m_state.m_allocator, &m_state.m_debugReport);
		vh::SetupPhysicalDevice(m_state.m_instance, m_device_extensions, &m_state.m_physicalDevice);
		vh::SetupGraphicsQueueFamily(m_state.m_physicalDevice, &m_state.m_queueFamily);
	    vh::SetupDevice( m_state.m_physicalDevice, nullptr, m_device_extensions, m_state.m_queueFamily, &m_state.m_device);
		//volkLoadDevice(m_device);

		vkGetDeviceQueue(m_state.m_device, m_state.m_queueFamily, 0, &m_state.m_queue);
		
		m_window->Init();
	};
	
	
	void Engine::RegisterSystem( std::shared_ptr<System> system, std::vector<MessageType> messageTypes) {
		for( auto messageType : messageTypes ) {
			m_messageMap[messageType].insert(system);
		}
	}

	void Engine::DeregisterSystem(std::shared_ptr<System> system) {
		for( auto& [messageType, systems] : m_messageMap ) {
			systems.erase(system);
		}
	}
	
	
	void Engine::LoadLevel( const char* levelName ){
		// Load level
	};
	
	void Engine::CreateWindow( const char* windowName, int width, int height ){
		m_window = std::make_unique<WindowSDL>(*this, m_state.m_instance, windowName, width, height, m_instance_extensions);
	};
	
	void Engine::CreateRenderer( const char* rendererName){
		// Create renderer
	};
	
	void Engine::CreateCamera( const char* cameraName ){
		// Create camera
	};
	
	void Engine::CreateSceneManager( const char* sceneManagerName ){
		// Create scene manager
	};
	
	void Engine::Run(){
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
	
	void Engine::Stop(){
		m_running = false;
	};
	
	
	void Engine::Shutdown(){
		m_renderer = nullptr;
		m_sceneManager = nullptr;
		m_window = nullptr;

	    auto PFN_DestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_state.m_instance, "vkDestroyDebugReportCallbackEXT");
	    PFN_DestroyDebugReportCallbackEXT(m_state.m_instance, m_state.m_debugReport, m_state.m_allocator);
	
	    vkDestroyDevice(m_state.m_device, m_state.m_allocator);
	    vkDestroyInstance(m_state.m_instance, m_state.m_allocator);
	}

};   // namespace vve

