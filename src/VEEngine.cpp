
#include <chrono>
#include <any>

#include "VHInclude.h"
#include "VHVulkan.h"
#include "VESystem.h"
#include "VEEngine.h"
#include "VEWindow.h"
#include "VEWindowSDL.h"
#include "VEGUI.h"
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
			{this, std::numeric_limits<int>::lowest(), MsgType::INIT, [this](Message message){this->OnInit(message);} },
			{this, std::numeric_limits<int>::max(),    MsgType::INIT, [this](Message message){this->OnInit2(message);} },
			{this, std::numeric_limits<int>::max(),    MsgType::QUIT, [this](Message message){this->OnQuit(message);} }
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
		CreateGUI();
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
		assert(!m_systems.contains(system->GetName()));
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
	void Engine<ATYPE>::CreateGUI() {
		RegisterSystem(std::make_unique<GUI<ATYPE>>("VVE GUI", this));
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Run(){
		m_running = true;
		Init();
		while(m_running) { Step(); }
		Quit();
	};

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Init(){
		SendMessage( MsgInit{this} );
		m_last = std::chrono::high_resolution_clock::now();
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Step(){
		auto now = std::chrono::high_resolution_clock::now();
		double dt = std::chrono::duration_cast<std::chrono::duration<double>>(now - m_last).count();
		m_last = now;
		SendMessage( MsgFrameStart{this, nullptr, dt} ) ;
		SendMessage( MsgPollEvents{this, nullptr, dt} ) ;
		SendMessage( MsgUpdate{this, nullptr, dt} ) ;
		SendMessage( MsgPrepareNextFrame{this, nullptr, dt} ) ;
		SendMessage( MsgRecordNextFrame{this, nullptr, dt} ) ;
		SendMessage( MsgRenderNextFrame{this, nullptr, dt} ) ;
		SendMessage( MsgPresentNextFrame{this, nullptr, dt} ) ;
		SendMessage( MsgFrameEnd{this, nullptr, dt} ) ;
	}

	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::Quit(){
		SendMessage( MsgQuit{this, nullptr} );
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

