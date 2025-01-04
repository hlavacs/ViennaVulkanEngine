
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
		RegisterSystem(std::make_unique<WindowSDL>(m_windowName, *this, m_windowName, 800, 600 ) );
	};
	
	void Engine::CreateRenderer(){
		RegisterSystem(std::make_unique<RendererVulkan>( m_rendererVulkanaName,  *this, m_windowName ) );
		RegisterSystem(std::make_unique<RendererImgui>(  m_rendererImguiName,   *this, m_windowName ) );
		RegisterSystem(std::make_unique<RendererForward>(m_rendererForwardName, *this, m_windowName) );
	};
	
	void Engine::CreateCamera( ){
		// Create camera
		/*auto window = GetWindow("VVE Window");
		Camera camera{(float)window->GetWidth() / (float)window->GetHeight()};
        auto view = glm::inverse( glm::lookAt(glm::vec3(2.0f, 1.9f, 1.8f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)) );
		//auto quat = glm::quat_cast(view);
		//auto pos = glm::vec3(view[3]);
		//auto rot = glm::mat3_cast(quat);
		auto h = m_registry.Insert(std::string("VVE Camera"), camera, SceneNode{{glm::vec3(view[3]), glm::quat_cast(view)}} );
		*/
	};
	
	void Engine::CreateSystems( ){
		RegisterSystem(std::make_unique<SceneManager>(m_sceneManagerName, *this));
	};

	void Engine::CreateGUI() {
		RegisterSystem(std::make_unique<GUI>(m_guiName, *this, m_windowName));
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

