
#include <chrono>

#include "VESystem.h"
#include "VEEngine.h"
#include "VHDevice.h"
#include "VEWindow.h"
#include "VEWindowSDL.h"
#include "VERendererForward.h"
#include "VERendererImgui.h"
#include "VESceneManager.h"

namespace vve {

	template<ArchitectureType ATYPE>
	Engine<ATYPE>::Engine() {
	#ifndef NDEBUG
		m_debug = true;
	#endif
		Init();
	};
	
	template<ArchitectureType ATYPE>
	Engine<ATYPE>::~Engine() {
		Shutdown();
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Init(){
		if(m_initialized) return;
		CreateWindow("Vulkan Engine", 800, 600);
		SetupVulkan();
		CreateRenderer("Forward");
		CreateCamera("Main Camera");
		CreateSceneManager("");
		LoadLevel("");
		m_initialized = true;
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::SetupVulkan() {

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
		
		for( auto& window : m_windows ) {
			window->Init();
		}
	};
	

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::RegisterSystem( System<ATYPE>* system, int priority, std::vector<MessageType> messageTypes) {
		for( auto messageType : messageTypes ) {
			auto& pm = m_messageMap[messageType];
			pm[priority] = system;
		}
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::DeregisterSystem(System<ATYPE>* system) {
		for( auto& map : m_messageMap ) {
			for( auto& [priority, sys] : map.second ) {
				if( sys == system ) {
					map.second.erase(priority);
				}
			}
		}
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::SendMessage( Message message ) {
		for( auto& [priority, system] : m_messageMap[message.GetType()] ) {
			system->ReceiveMessage(message);
		}
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::LoadLevel( const char* levelName ){
		// Load level
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateWindow( const char* windowName, int width, int height ){
		m_windows.push_back( std::make_shared<WindowSDL<ATYPE>>("WindowSDL", *this, m_state.m_instance, windowName, width, height, m_instance_extensions) );
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateRenderer( const char* rendererName){
		m_windows[0]->AddRenderer(100, std::make_shared<RendererImgui<ATYPE>>("RendererImgui", *this, m_windows[0]) );
		m_windows[0]->AddRenderer(10, std::make_shared<RendererForward<ATYPE>>("RendererForward", *this, m_windows[0]) );
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateCamera( const char* cameraName ){
		// Create camera
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateSceneManager( const char* sceneManagerName ){
		m_sceneManager = std::make_shared<SceneManager<ATYPE>>(*this);
	};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Run(){
		Init();
		std::clock_t start = std::clock();
		m_running = true;
		auto last = std::chrono::high_resolution_clock::now();
		while(m_running) { //call stop to stop the engine
			auto now = std::chrono::high_resolution_clock::now();
			auto dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - last).count();

			SendMessage( MessageFrameStart{dt} ) ;
			SendMessage( MessagePollEvents{dt} ) ;
			SendMessage( MessageUpdate{dt} ) ;
			SendMessage( MessagePrepareNextFrame{dt} ) ;
			SendMessage( MessageDrawGUI{} ) ;
			SendMessage( MessageRenderNextFrame{dt} ) ;
			SendMessage( MessageShowNextFrame{dt} ) ;
			SendMessage( MessageFrameEnd{dt} ) ;
		}
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Stop(){
		m_running = false;
	};
	
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Shutdown(){
		m_sceneManager = nullptr;
		m_windows.clear();

	    auto PFN_DestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_state.m_instance, "vkDestroyDebugReportCallbackEXT");
	    PFN_DestroyDebugReportCallbackEXT(m_state.m_instance, m_state.m_debugReport, m_state.m_allocator);
	
	    vkDestroyDevice(m_state.m_device, m_state.m_allocator);
	    vkDestroyInstance(m_state.m_instance, m_state.m_allocator);
	}

	template class Engine<ArchitectureType::SEQUENTIAL>;
	template class Engine<ArchitectureType::PARALLEL>;

};   // namespace vve

