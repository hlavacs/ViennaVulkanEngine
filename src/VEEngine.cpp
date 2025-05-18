
#define VIENNA_VULKAN_HELPER_IMPL

#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {
	
	Engine::Engine(std::string name, uint32_t apiVersion, bool debug) : System(name, *this), m_apiVersion(apiVersion) {
		if( VK_VERSION_MAJOR(apiVersion) == 1 && VK_VERSION_MINOR(apiVersion) < VK_VERSION_MINOR(c_minimumVersion)) {
			std::cout << "Minimum VVE Vulkan API version is 1." << VK_VERSION_MINOR(c_minimumVersion) << "!\n";
			m_apiVersion = c_minimumVersion;
		}

		if( VK_VERSION_MAJOR(apiVersion) == 1 && VK_VERSION_MINOR(apiVersion) > VK_VERSION_MINOR(c_maximumVersion)) {
			std::cout << "Maximum VVE Vulkan API version is 1." << VK_VERSION_MINOR(c_maximumVersion) << "!\n";
			m_apiVersion = c_maximumVersion;
		}

	#ifndef NDEBUG
		m_debug = true;
	#endif
		m_debug = m_debug | debug;
		auto transform = [&](const std::string& str) { m_msgTypeMap[std::hash<std::string>{}(str)] = str; };
		std::ranges::for_each( MsgTypeNames, transform );
	};
	
	Engine::~Engine() {};

	void Engine::RegisterCallbacks( std::vector<MessageCallback> callbacks) {
		for( auto& callback : callbacks ) {
			assert(MsgTypeNames.contains(callback.m_messageName));
			auto& pm = m_messageMap[std::hash<std::string>{}(callback.m_messageName)];
			pm.insert({callback.m_phase, callback});
		}
	}

	void Engine::DeregisterCallbacks(System* system, std::string messageName) {
		auto& pm = m_messageMap[std::hash<std::string>{}(messageName)];
		for (auto it = pm.begin(); it != pm.end();) {
			if( it->second.m_system == system ) {
				it = pm.erase(it);
			} else ++it;
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

	void Engine::SendMsg( Message message ) {
		for( auto& [phase, callback] : m_messageMap[message.GetType()] ) {
			message.SetPhase(phase);
			if( callback.m_callback(message) ) { return; }
		}
	}
	
	void Engine::CreateWindows(){
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
			CreateWindows();
			CreateRenderer();
			CreateSystems();
			CreateGUI();		
			SendMsg( MsgInit{} );
			SendMsg( MsgLoadLevel{""} );
		}
		m_initialized = true;
		m_last = std::chrono::high_resolution_clock::now();
	}

	void Engine::Step(){
		auto now = std::chrono::high_resolution_clock::now();
		double dt = std::chrono::duration<double, std::micro>(now - m_last).count() / 1'000'000.0;
		m_last = now;

		SendMsg( MsgFrameStart{dt} ) ;
		SendMsg( MsgPollEvents{dt} ) ;
		SendMsg( MsgUpdate{dt} ) ;

		auto [handle, stateW, stateSDL] = WindowSDL::GetState(m_registry);

		if(!stateW().m_isMinimized) {
			SendMsg( MsgPrepareNextFrame{dt} ) ;
			SendMsg( MsgRecordNextFrame{dt} ) ;
			SendMsg( MsgRenderNextFrame{dt} ) ;
			SendMsg( MsgPresentNextFrame{dt} ) ;
		}
		SendMsg( MsgFrameEnd{dt} ) ;
	}

	void Engine::Quit(){
		Message( MsgQuit{} );
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

	//-------------------------------------------------------------------------------------------------------------------
	// simple interface

	auto Engine::GetHandle(std::string name) -> vecs::Handle { 
		return m_handleMap[name]; 
	}

	auto Engine::SetHandle(std::string name, vecs::Handle h) -> void {
		m_handleMap[name] = h;
	}

	auto  Engine::ContainsHandle(std::string name) -> bool {
		return m_handleMap.contains(name);
	}

	//-------------------------------------------------------------------------------------------------------------------

	auto Engine::GetRootSceneNode() -> ObjectHandle { 
		return ObjectHandle{ GetHandle(SceneManager::m_rootName) };
	};

	auto Engine::CreateSceneNode(Name name, ParentHandle parent, Position position, Rotation rotation, Scale scale) -> ObjectHandle {
		if( !parent().IsValid() ) parent = GetRootSceneNode();
		ObjectHandle node{ m_registry.Insert( name, parent, Children{},	position, rotation, scale) };
		m_engine.SetParent(node, parent);
		return node;
	}

	auto Engine::GetParent(ObjectHandle object) -> ParentHandle{ 
		return m_registry.Get<ParentHandle>(object);
	};

	void Engine::SetParent(ObjectHandle object, ParentHandle parent) { 
		m_engine.SendMsg(MsgObjectSetParent{object, parent});
	};

	//-------------------------------------------------------------------------------------------------------------------
	
	void Engine::LoadScene(const Filename& filename, aiPostProcessSteps flags) { 
		m_engine.SendMsg( MsgSceneLoad{ filename, flags });
	};
	
	auto Engine::CreateScene(const Filename& filename, aiPostProcessSteps flags, ParentHandle parent, 
							Position position, Rotation rotation, Scale scale) -> ObjectHandle {
		ObjectHandle handle{ m_registry.Insert( position, rotation, scale) };
        m_engine.SendMsg(MsgSceneCreate{ handle, parent, filename, flags });
		return handle;
	};

	auto Engine::CreateObject(	const MeshName& meshName, vvh::Color color, ParentHandle parent, 	
								Position position, Rotation rotation, Scale scale, UVScale uvScale) -> ObjectHandle { 
            ObjectHandle handle{ m_registry.Insert(position, rotation, scale, color, meshName, uvScale) };
            m_engine.SendMsg(MsgObjectCreate{  handle, vve::ParentHandle{} });
			return handle;
    	};

	auto Engine::CreateObject(	const MeshName& meshName, const TextureName& textureName, ParentHandle parent, 	
								Position position, Rotation rotation, Scale scale, UVScale uvScale) -> ObjectHandle { 
            ObjectHandle handle{ m_registry.Insert(position, rotation, scale, meshName, textureName, uvScale) };
            m_engine.SendMsg(MsgObjectCreate{  handle, vve::ParentHandle{} });
			return handle;
    	};

	void Engine::DestroyObject(ObjectHandle handle) {
		m_engine.SendMsg(MsgObjectDestroy{handle});
	};

	//-------------------------------------------------------------------------------------------------------------------

	auto Engine::CreateSpotLight() -> vecs::Handle{ return {}; };
	auto Engine::CreateDirectionalLight() -> vecs::Handle{ return {}; };
	auto Engine::CreatePointLight() -> vecs::Handle{ return {}; };

	auto Engine::CreateCamera(Name name, ParentHandle parent, Camera camera, Position position, Rotation rotation, Scale scale) -> ObjectHandle { 
		ObjectHandle handle{ m_registry.Insert( name, parent, camera, position, rotation, scale )};
		m_engine.SetParent(handle, parent);
		return handle;
	};

	//-------------------------------------------------------------------------------------------------------------------

	auto GetLocalToParentTransform() -> mat4_t { return {}; };
	auto GetLocalToWorldTransform() -> mat4_t { return {}; };
	auto Engine::GetPosition(){};
	auto Engine::GetOrientation(){};
	auto Engine::GetScale(){};
	auto Engine::GetUVScale(){};
	auto SetLocalToParentTransform() {};
	auto SetLocalToWorldTransform() {};
	auto Engine::SetPosition(){};
	auto Engine::SetOrientation(){};
	auto Engine::SetScale(){};
	auto Engine::SetUVScale(){};

	//-------------------------------------------------------------------------------------------------------------------

	auto Engine::CreateMesh() -> vecs::Handle{ return {}; };
	auto Engine::CreateTexture() -> vecs::Handle{ return {}; };
	auto Engine::CreateMaterial() -> vecs::Handle{ return {}; };
	void Engine::EraseMaterial(){};
	void Engine::EraseTexture(){};
	void Engine::EraseMesh(){}

	//-------------------------------------------------------------------------------------------------------------------

	void Engine::PlaySound(const Filename& filename, int mode, float volume) {
        m_engine.SendMsg(MsgPlaySound{ filename, mode, (int)volume });
	};

	void Engine::SetVolume(float volume) {
		m_engine.SendMsg(MsgSetVolume{ (int)volume });

	};

};   // namespace vve

