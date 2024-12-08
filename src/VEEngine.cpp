
#include <chrono>
#include <any>

#include "VHInclude.h"
#include "VHVulkan.h"
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
	Engine<ATYPE>::Engine(std::string name) : System<ATYPE>(name, this) {
	#ifndef NDEBUG
		m_debug = true;
	#endif
		RegisterCallback( { 
			{this, std::numeric_limits<int>::lowest(), MessageType::INIT, [this](Message message){this->OnInit(message);} },
			{this, std::numeric_limits<int>::max(),    MessageType::INIT, [this](Message message){this->OnInit2(message);} },
			{this, std::numeric_limits<int>::max(),    MessageType::QUIT, [this](Message message){this->OnQuit(message);} }
		} );
	};
	
	template<ArchitectureType ATYPE>
	Engine<ATYPE>::~Engine() {};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::OnInit(Message message ) {
		CreateWindow();
		CreateRenderer();
		CreateSystems();
		CreateCamera();
	};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::OnInit2(Message message ) {
		LoadLevel("");
	};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::RegisterCallback( std::vector<MessageCallback> callbacks) {
		for( auto& callback : callbacks ) {
			auto& pm = m_messageMap[callback.m_messageType];
			pm.insert({callback.m_phase, callback});
		}
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::RegisterSystem( std::unique_ptr<System<ATYPE>>&& system ) {
		m_systems[system->GetName()] = std::move(system);
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
	void Engine<ATYPE>::CreateWindow(){
		RegisterSystem(std::make_unique<WindowSDL<ATYPE>>(GetMainWindowName(), this, "Vulkane Engine", 800, 600 ) );
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateRenderer(){
		RegisterSystem(std::make_unique<RendererVulkan<ATYPE>>( "VVE RendererVulkan",  this, GetMainWindow() ) );
		RegisterSystem(std::make_unique<RendererImgui<ATYPE>>(  "VVE RendererImgui",   this, GetMainWindow() ) );
		RegisterSystem(std::make_unique<RendererForward<ATYPE>>("VVE RendererForward", this, GetMainWindow() ) );
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateCamera( ){
		// Create camera
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateSystems( ){
		RegisterSystem(std::make_unique<SceneManager<ATYPE>>("VVE SceneManager", this));
	};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Run(){
		m_running = true;
		Init();
		while(m_running) { Step(); }
		Quit();
	};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Init(){
		SendMessage( MessageInit{this} );
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Step(){
		auto last = std::chrono::high_resolution_clock::now();
		SendMessage( MessageFrameStart{this, nullptr, m_dt} ) ;
		SendMessage( MessagePollEvents{this, nullptr, m_dt} ) ;
		SendMessage( MessageUpdate{this, nullptr, m_dt} ) ;
		SendMessage( MessagePrepareNextFrame{this, nullptr, m_dt} ) ;
		SendMessage( MessageRecordNextFrame{this, nullptr, m_dt} ) ;
		SendMessage( MessageRenderNextFrame{this, nullptr, m_dt} ) ;
		SendMessage( MessagePresentNextFrame{this, nullptr, m_dt} ) ;
		SendMessage( MessageFrameEnd{this, nullptr, m_dt} ) ;
		auto now = std::chrono::high_resolution_clock::now();
		m_dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - last).count();
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Quit(){
		SendMessage( MessageQuit{this, nullptr} );
	}

	template<ArchitectureType ATYPE>
	auto Engine<ATYPE>::GetSystem( std::string name ) -> System<ATYPE>* { 
		auto system = m_systems[name].get();
		assert(system != nullptr);
		return system;
	}	

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Stop() {
		m_running = false;
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::OnQuit(Message message) {}

	template class Engine<ENGINETYPE_SEQUENTIAL>;
	template class Engine<ENGINETYPE_PARALLEL>;

};   // namespace vve

