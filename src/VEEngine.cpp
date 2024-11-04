
#include <chrono>

#include "VHInclude.h"
#include "VESystem.h"
#include "VEEngine.h"
#include "VEWindow.h"
#include "VEWindowSDL.h"
#include "VERendererForward.h"
#include "VERendererImgui.h"
#include "VERendererVulkan.h"
#include "VESceneManager.h"

namespace vve {

	template<ArchitectureType ATYPE>
	Engine<ATYPE>::Engine(std::string name) : System<ATYPE>(this, name) {
	#ifndef NDEBUG
		m_debug = true;
	#endif
		RegisterSystem( this, std::numeric_limits<int>::lowest(), {MessageType::INIT} );
		RegisterSystem( this, std::numeric_limits<int>::max(), {MessageType::QUIT} );
		
		CreateWindow("Vulkan Engine", 800, 600);
		SetupVulkan();
		CreateRenderer("Forward");
		CreateSceneManager("");
		CreateCamera("Main Camera");
	};
	
	template<ArchitectureType ATYPE>
	Engine<ATYPE>::~Engine() {};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::OnInit(Message message ) {
		LoadLevel("");
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
	};
	

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::RegisterSystem( System<ATYPE>* system, int phase, std::vector<MessageType> messageTypes) {
		for( auto messageType : messageTypes ) {
			auto& pm = m_messageMap[messageType];
			pm.insert({phase, system});
		}
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::DeregisterSystem(System<ATYPE>* system) {
		for( auto& map : m_messageMap ) {
			for( auto iter = map.second.begin(); iter != map.second.end(); ) {
				if( iter->second == system ) {
					iter = map.second.erase(iter);
				} else {
					++iter;
				}
			}
		}
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::SendMessage( Message message ) {
		for( auto& [phase, system] : m_messageMap[message.GetType()] ) {
			message.SetPhase(phase);
			void* receiver = message.GetReceiver();
			if( receiver == nullptr || receiver == system ) [[likely]]
				system->ReceiveMessage(message);
		}
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::LoadLevel( const char* levelName ){
		// Load level
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateWindow( const char* windowName, int width, int height ){
		m_windows.push_back( std::make_shared<WindowSDL<ATYPE>>(this, m_state.m_instance, windowName, width, height, m_instance_extensions) );
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateRenderer( const char* rendererName){
		m_windows[0]->AddRenderer(std::make_shared<RendererVulkan<ATYPE>>(this, m_windows[0].get()) );
		m_windows[0]->AddRenderer(std::make_shared<RendererImgui<ATYPE>>(this, m_windows[0].get()) );
		m_windows[0]->AddRenderer(std::make_shared<RendererForward<ATYPE>>(this, m_windows[0].get()) );
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateCamera( const char* cameraName ){
		// Create camera
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateSceneManager( const char* sceneManagerName ){
		m_sceneManager = std::make_shared<SceneManager<ATYPE>>(this);
	};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Run(){
		SendMessage( MessageInit{this} );
		
		std::clock_t start = std::clock();
		m_running = true;
		auto last = std::chrono::high_resolution_clock::now();
		while(m_running) { //call stop to stop the engine
			auto now = std::chrono::high_resolution_clock::now();
			auto dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - last).count();

			SendMessage( MessageFrameStart{this, nullptr, dt} ) ;
			SendMessage( MessagePollEvents{this, nullptr, dt} ) ;
			SendMessage( MessageUpdate{this, nullptr, dt} ) ;
			SendMessage( MessagePrepareNextFrame{this, nullptr, dt} ) ;
			SendMessage( MessageRecordNextFrame{this, nullptr, dt} ) ;
			SendMessage( MessageRenderNextFrame{this, nullptr, dt} ) ;
			SendMessage( MessagePresentNextFrame{this, nullptr, dt} ) ;
			SendMessage( MessageFrameEnd{this, nullptr, dt} ) ;
		}
		SendMessage( MessageQuit{this, nullptr} ) ;
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Stop() {
		m_running = false;
	};
	
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::OnQuit(Message message) {
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

