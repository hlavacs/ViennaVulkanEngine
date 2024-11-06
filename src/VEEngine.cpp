
#include <chrono>

#include <any>
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
		RegisterSystem( { 
			  {this, std::numeric_limits<int>::lowest(), MessageType::INIT, [this](Message message){this->OnInit(message);} }
			, {this, std::numeric_limits<int>::max(),    MessageType::INIT, [this](Message message){this->OnInit2(message);} }
			, {this, std::numeric_limits<int>::max(),    MessageType::QUIT, [this](Message message){this->OnQuit(message);} }
		} );
	};
	
	template<ArchitectureType ATYPE>
	Engine<ATYPE>::~Engine() {};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::OnInit(Message message ) {
		CreateWindow("Vulkan Engine", 800, 600);
		CreateRenderer("Forward");
		CreateSceneManager("");
		CreateCamera("Main Camera");
	};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::OnInit2(Message message ) {
		LoadLevel("");
	};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::RegisterSystem( std::vector<MessageCallback> callbacks) {
		for( auto& callback : callbacks ) {
			auto& pm = m_messageMap[callback.m_messageType];
			pm.insert({callback.m_phase, callback});
			m_systems[callback.m_system->GetName()] = callback.m_system;
		}
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::DeregisterSystem(System<ATYPE>* system) {
		for( auto& map : m_messageMap ) {
			for( auto iter = map.second.begin(); iter != map.second.end(); ) {
				if( iter->second.m_system == system ) {
					iter = map.second.erase(iter);
				} else {
					++iter;
				}
			}
		}
		m_systems.erase(system->GetName());
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::SendMessage( Message message ) {
		for( auto& [phase, callback] : m_messageMap[message.GetType()] ) {
			message.SetPhase(phase);
			void* receiver = message.GetReceiver();
			if( receiver == nullptr || receiver == callback.m_system ) [[likely]]
				callback.m_callback(message);
		}
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::LoadLevel( std::string levelName ){
		// Load level
		std::cout << "Loading level: " << levelName << std::endl;
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateWindow( const char* windowName, int width, int height ){
		m_windows.push_back(std::make_shared<WindowSDL<ATYPE>>(this, windowName, width, height ) );
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
	void Engine<ATYPE>::OnQuit(Message message) {}

	template class Engine<ArchitectureType::SEQUENTIAL>;
	template class Engine<ArchitectureType::PARALLEL>;

};   // namespace vve

