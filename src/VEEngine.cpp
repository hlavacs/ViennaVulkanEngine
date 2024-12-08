
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
	Engine<ATYPE>::Engine(std::string name) : System<ATYPE>(this, name) {
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
		RegisterSystem(std::make_unique<WindowSDL<ATYPE>>(this, "Vulkane Engine", 800, 600, "Main Window" ) );
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateRenderer(){
		RegisterSystem(std::make_unique<RendererVulkan<ATYPE>>( this, GetWindow("Main Window"), "VVE RendererVulkan") );
		RegisterSystem(std::make_unique<RendererImgui<ATYPE>>(  this, GetWindow("Main Window"), "VVE RendererImgui") );
		RegisterSystem(std::make_unique<RendererForward<ATYPE>>(this, GetWindow("Main Window"), "VVE RendererForward") );
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateCamera( ){
		// Create camera
	};
	
	template<ArchitectureType ATYPE>
	void Engine<ATYPE>::CreateSystems( ){
		RegisterSystem(std::make_unique<SceneManager<ATYPE>>(this, "VVE SceneManager"));
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

