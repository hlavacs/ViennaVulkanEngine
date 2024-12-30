
#include <chrono>
#include <any>

#include <algorithm>
#include <iterator>
#include <ranges>
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {
	
	Engine::Engine(std::string name) : System(name, *this) {
	#ifndef NDEBUG
		m_debug = true;
	#endif
		auto transform = [&](const auto& str) { m_msgTypeMap[std::hash<std::string>{}(str)] = str; };
		std::ranges::for_each( MsgTypeNames, transform );
	};
	
	Engine::~Engine() {};

	void Engine::RegisterCallback( std::vector<MessageCallback> callbacks) {
		for( auto& callback : callbacks ) {
			assert(MsgTypeNames.contains(callback.m_messageName));
			auto& pm = m_messageMap[std::hash<std::string>{}(callback.m_messageName)];
			pm.insert({callback.m_phase, callback});
		}
	}

	void Engine::RegisterSystem( std::unique_ptr<System>&& system ) {
		assert(!m_systems.contains(system->GetName()));
		m_systems[system->GetName()] = std::move(system);
	}

	void Engine::DeregisterSystem(System* system) {
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

	void Engine::SendMessage( Message message ) {
		for( auto& [phase, callback] : m_messageMap[message.GetType()] ) {
			message.SetPhase(phase);
			void* receiver = message.GetReceiver();
			if( receiver == nullptr || receiver == callback.m_system ) [[likely]]
				callback.m_callback(message);
		}
	}

	void Engine::LoadLevel( std::string levelName ) {
		// Load level
		std::cout << "Loading level: " << levelName << std::endl;
	};
	
	void Engine::CreateWindow(){
		RegisterSystem(std::make_unique<WindowSDL>("VVE Window", *this, "Vienna Vulkan Engine", 800, 600 ) );
	};
	
	void Engine::CreateRenderer(){
		RegisterSystem(std::make_unique<RendererVulkan>( "VVE Renderer Vulkan",  *this, "VVE Window" ) );
		RegisterSystem(std::make_unique<RendererImgui>(  "VVE Renderer Imgui",   *this, "VVE Window" ) );
		RegisterSystem(std::make_unique<RendererForward>("VVE Renderer Forward", *this, "VVE Window") );
	};
	
	void Engine::CreateCamera( ){
		// Create camera
	};
	
	void Engine::CreateSystems( ){
		RegisterSystem(std::make_unique<SceneManager>("VVE SceneManager", *this));
	};

	void Engine::CreateGUI() {
		RegisterSystem(std::make_unique<GUI>("VVE GUI", *this, "VVE Window"));
	}

	void Engine::Run(){
		m_running = true;
		Init();
		while(m_running) { Step(); }
		Quit();
	};

	void Engine::Init() {
		if(!m_initialized) {
			CreateWindow();
			CreateRenderer();
			CreateSystems();
			CreateCamera();
			CreateGUI();		
			SendMessage( MsgAnnounce{this} );
			SendMessage( MsgInit{this} );
			LoadLevel("");
		}
		m_initialized = true;
		m_last = std::chrono::high_resolution_clock::now();
	}

	void Engine::Step(){
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

	void Engine::Quit(){
		SendMessage( MsgQuit{this, nullptr} );
	}

	auto Engine::GetSystem( std::string name ) -> System* { 
		auto system = m_systems[name].get();
		assert(system != nullptr);
		return system;
	}	

	void Engine::Stop() {
		m_running = false;
	};
	
	void Engine::PrintCallbacks() {
		if( !m_debug ) return;
		for( auto& [type, map] : m_messageMap ) {
			std::cout << "Message Type: " << m_msgTypeMap[type] << std::endl;
			for( auto& [phase, callback] : map ) {
				std::cout << "  Phase: " << std::setw(11) << phase << " System: '" << callback.m_system->GetName() << "'" << std::endl;
			}
			std::cout << std::endl;
		}
	}

};   // namespace vve

