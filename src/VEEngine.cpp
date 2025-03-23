
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {
	
	Engine::Engine(std::string name, uint32_t apiVersion, bool debug) : System(name, *this), m_apiVersion(apiVersion) {
		if( VK_VERSION_MAJOR(apiVersion) == 1 && VK_VERSION_MINOR(apiVersion) < VK_VERSION_MINOR(c_minimumVersion)) {
			std::cout << "Minimum VVE Vulkan API version is 1." << VK_VERSION_MINOR(c_minimumVersion) << "!\n";
			m_apiVersion = c_minimumVersion;
		}

	#ifndef NDEBUG
		m_debug = true;
	#endif
		m_debug = m_debug | debug;
		auto transform = [&](const std::string& str) { m_msgTypeMap[std::hash<std::string>{}(str)] = str; };
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
			if( callback.m_callback(message) ) { return; }
		}
	}
	
	void Engine::CreateWindow(){
		RegisterSystem(std::make_unique<WindowSDL>(m_windowName, *this, m_windowName, 1200, 600 ) );
	};
	
	void Engine::CreateRenderer(){
		RegisterSystem(std::make_unique<RendererVulkan>( m_rendererVulkanaName,  *this, m_windowName ) );
		RegisterSystem(std::make_unique<RendererImgui>(  m_rendererImguiName,   *this, m_windowName ) );
		RegisterSystem(std::make_unique<RendererForward>(m_rendererForwardName, *this, m_windowName) );
	};
	
	void Engine::CreateSystems( ){
		RegisterSystem(std::make_unique<SceneManager>(m_sceneManagerName, *this));
		RegisterSystem(std::make_unique<AssetManager>(m_assetManagerName, *this));
		RegisterSystem(std::make_unique<SoundManager>(m_soundManagerName, *this));
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
			CreateGUI();		
			SendMessage( MsgInit{} );
			SendMessage( MsgLoadLevel{""} );
		}
		m_initialized = true;
		m_last = std::chrono::high_resolution_clock::now();
	}

	void Engine::Step(){
		auto now = std::chrono::high_resolution_clock::now();
		double dt = std::chrono::duration<double, std::micro>(now - m_last).count() / 1'000'000.0;
		m_last = now;

		SendMessage( MsgFrameStart{dt} ) ;
		SendMessage( MsgPollEvents{dt} ) ;
		SendMessage( MsgUpdate{dt} ) ;
		SendMessage( MsgPrepareNextFrame{dt} ) ;
		SendMessage( MsgRecordNextFrame{dt} ) ;
		SendMessage( MsgRenderNextFrame{dt} ) ;
		SendMessage( MsgPresentNextFrame{dt} ) ;
		SendMessage( MsgFrameEnd{dt} ) ;
	}

	auto Engine::GetHandle(std::string name) -> vecs::Handle { 
		return m_handleMap[name]; 
	}

	auto Engine::SetHandle(std::string name, vecs::Handle h) -> void {
		m_handleMap[name] = h;
	}

	auto  Engine::ContainsHandle(std::string name) -> bool {
		return m_handleMap.contains(name);
	}

	void Engine::Quit(){
		SendMessage( MsgQuit{} );
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

