
#define VIENNA_VULKAN_HELPER_IMPL

#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

	/**
	 * @brief Constructor for the Engine class
	 * @param name Name of the engine instance
	 * @param type Type of renderer to use (forward or deferred)
	 * @param apiVersion Vulkan API version to use
	 * @param debug Enable debug mode
	 */
	Engine::Engine(std::string name, RendererType type, uint32_t apiVersion, bool debug) : System(name, *this), m_apiVersion(apiVersion) {
		if( VK_VERSION_MAJOR(apiVersion) == 1 && VK_VERSION_MINOR(apiVersion) < VK_VERSION_MINOR(c_minimumVersion)) {
			std::cout << "Minimum VVE Vulkan API version is 1." << VK_VERSION_MINOR(c_minimumVersion) << "!\n";
			m_apiVersion = c_minimumVersion;
		}

		if( VK_VERSION_MAJOR(apiVersion) == 1 && VK_VERSION_MINOR(apiVersion) > VK_VERSION_MINOR(c_maximumVersion)) {
			std::cout << "Maximum VVE Vulkan API version is 1." << VK_VERSION_MINOR(c_maximumVersion) << "!\n";
			m_apiVersion = c_maximumVersion;
		}

		m_type = type;

	#ifndef NDEBUG
		m_debug = true;
	#endif
		m_debug = m_debug | debug;
		auto transform = [&](const std::string& str) { m_msgTypeMap[std::hash<std::string>{}(str)] = str; };
		std::ranges::for_each( MsgTypeNames, transform );
	};

	/**
	 * @brief Destructor for the Engine class
	 */
	Engine::~Engine() {};

	/**
	 * @brief Register message callbacks for the engine
	 * @param callbacks Vector of message callbacks to register
	 */
	void Engine::RegisterCallbacks( std::vector<MessageCallback> callbacks) {
		for( auto& callback : callbacks ) {
			assert(MsgTypeNames.contains(callback.m_messageName));
			auto& pm = m_messageMap[std::hash<std::string>{}(callback.m_messageName)];
			pm.insert({callback.m_phase, callback});
		}
	}

	/**
	 * @brief Deregister callbacks for a specific system and message name
	 * @param system Pointer to the system whose callbacks should be removed
	 * @param messageName Name of the message type
	 */
	void Engine::DeregisterCallbacks(System* system, std::string messageName) {
		auto& pm = m_messageMap[std::hash<std::string>{}(messageName)];
		for (auto it = pm.begin(); it != pm.end();) {
			if( it->second.m_system == system ) {
				it = pm.erase(it);
			} else ++it;
		}
	}

	/**
	 * @brief Register a system with the engine
	 * @param system Unique pointer to the system to register
	 */
	void Engine::RegisterSystem( std::unique_ptr<System>&& system ) {
		assert(!m_systems.contains(system->GetName()));
		m_systems[system->GetName()] = std::move(system);
	}

	/**
	 * @brief Deregister a system from the engine
	 * @param system Pointer to the system to deregister
	 */
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

	/**
	 * @brief Send a message to all registered callbacks
	 * @param message Message to send
	 */
	void Engine::SendMsg( Message message ) {
		for( auto& [phase, callback] : m_messageMap[message.GetType()] ) {
			message.SetPhase(phase);
			if( callback.m_callback(message) ) { return; }
		}
	}

	/**
	 * @brief Create and register the window system
	 */
	void Engine::CreateWindows(){
		RegisterSystem(std::make_unique<WindowSDL>(m_windowName, *this, m_windowName, 1200, 600 ) );
	};

	/**
	 * @brief Create and register the renderer systems
	 */
	void Engine::CreateRenderer(){
		RegisterSystem(std::make_unique<RendererVulkan>( m_rendererVulkanName,  *this, m_windowName ) );
		RegisterSystem(std::make_unique<RendererImgui>(  m_rendererImguiName,   *this, m_windowName ) );
		if (m_type == vve::RendererType::RENDERER_TYPE_FORWARD)
			RegisterSystem(std::make_unique<RendererForward>(m_rendererForwardName, *this, m_windowName) );
#ifdef VVE_GAUSSIAN_ENABLED
		else if (m_type == vve::RendererType::RENDERER_TYPE_GAUSSIAN)
			RegisterSystem(std::make_unique<RendererGaussian>(m_rendererGaussianName, *this, m_windowName) );
#endif
		else
			RegisterSystem(std::make_unique<RendererDeferred>(m_rendererDeferredName, *this, m_windowName) );
	};

	/**
	 * @brief Create and register core engine systems (scene, asset, sound managers)
	 */
	void Engine::CreateSystems( ){
		RegisterSystem(std::make_unique<SceneManager>(m_sceneManagerName, *this));
		RegisterSystem(std::make_unique<AssetManager>(m_assetManagerName, *this));
		RegisterSystem(std::make_unique<SoundManager>(m_soundManagerName, *this));
	};

	/**
	 * @brief Create and register the GUI system
	 */
	void Engine::CreateGUI() {
		RegisterSystem(std::make_unique<GUI>(m_guiName, *this, m_windowName));
	}

	/**
	 * @brief Main engine run loop - initializes, runs update loop, then quits
	 */
	void Engine::Run(){
		m_running = true;
		Init();
		while(m_running) { Step(); }
		Quit();
	};

	/**
	 * @brief Initialize the engine and all systems
	 */
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

	/**
	 * @brief Execute a single frame update step
	 */
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

	/**
	 * @brief Quit the engine and send quit message
	 */
	void Engine::Quit(){
		Message( MsgQuit{} );
	}

	/**
	 * @brief Stop the engine's main loop
	 */
	void Engine::Stop() {
		m_running = false;
	};

	/**
	 * @brief Print all registered callbacks (debug only)
	 */
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

	/**
	 * @brief Get a handle by name from the handle map
	 * @param name Name of the handle
	 * @return Handle associated with the name
	 */
	auto Engine::GetHandle(std::string name) -> vecs::Handle { 
		return m_handleMap[name];
	}

	/**
	 * @brief Set a handle by name in the handle map
	 * @param name Name of the handle
	 * @param h Handle to associate with the name
	 */
	auto Engine::SetHandle(std::string name, vecs::Handle h) -> void {
		m_handleMap[name] = h;
	}

	/**
	 * @brief Check if a handle with the given name exists
	 * @param name Name of the handle to check
	 * @return True if handle exists, false otherwise
	 */
	auto  Engine::ContainsHandle(std::string name) -> bool {
		return m_handleMap.contains(name);
	}

	//-------------------------------------------------------------------------------------------------------------------

	/**
	 * @brief Get the root scene node
	 * @return Handle to the root scene node
	 */
	auto Engine::GetRootSceneNode() -> ObjectHandle { 
		return ObjectHandle{ GetHandle(SceneManager::m_rootName) };
	};

	/**
	 * @brief Create a new scene node
	 * @param name Name of the scene node
	 * @param parent Parent node handle
	 * @param position Initial position
	 * @param rotation Initial rotation
	 * @param scale Initial scale
	 * @return Handle to the created scene node
	 */
	auto Engine::CreateSceneNode(Name name, ParentHandle parent, Position position, Rotation rotation, Scale scale) -> ObjectHandle {
		if( !parent().IsValid() ) parent = GetRootSceneNode();
		ObjectHandle node{ m_registry.Insert( name, parent, Children{},	position, rotation, scale) };
		m_engine.SetParent(node, parent);
		return node;
	}

	/**
	 * @brief Get the parent of an object
	 * @param object Handle to the object
	 * @return Handle to the parent object
	 */
	auto Engine::GetParent(ObjectHandle object) -> ParentHandle { 
		return m_registry.Get<ParentHandle>(object);
	};

	/**
	 * @brief Set the parent of an object
	 * @param object Handle to the object
	 * @param parent Handle to the new parent
	 */
	void Engine::SetParent(ObjectHandle object, ParentHandle parent) { 
		if( !parent().IsValid() ) parent = GetRootSceneNode();
		m_engine.SendMsg(MsgObjectSetParent{object, parent});
	};

	//-------------------------------------------------------------------------------------------------------------------

	/**
	 * @brief Load a scene from a file
	 * @param filename Path to the scene file
	 * @param flags Assimp post-process flags
	 */
	void Engine::LoadScene(const Filename& filename, aiPostProcessSteps flags) { 
		m_engine.SendMsg( MsgSceneLoad{ filename, flags });
	};

	/**
	 * @brief Create a scene from a file with transform
	 * @param name Name of the scene
	 * @param parent Parent node handle
	 * @param filename Path to the scene file
	 * @param flags Assimp post-process flags
	 * @param position Initial position
	 * @param rotation Initial rotation
	 * @param scale Initial scale
	 * @return Handle to the created scene
	 */
	auto Engine::CreateScene(Name name, ParentHandle parent, const Filename& filename, aiPostProcessSteps flags,
							Position position, Rotation rotation, Scale scale) -> ObjectHandle {
		ObjectHandle handle{ m_registry.Insert( position, rotation, scale) };
        m_engine.SendMsg(MsgSceneCreate{ handle, parent, filename, flags });
		return handle;
	};

	/**
	 * @brief Create an object with a mesh and color
	 * @param name Name of the object
	 * @param parent Parent node handle
	 * @param meshName Name of the mesh
	 * @param color Object color
	 * @param position Initial position
	 * @param rotation Initial rotation
	 * @param scale Initial scale
	 * @param uvScale UV scale
	 * @return Handle to the created object
	 */
	auto Engine::CreateObject(	Name name, ParentHandle parent, const MeshName& meshName, vvh::Color color,
								Position position, Rotation rotation, Scale scale, UVScale uvScale) -> ObjectHandle { 
            ObjectHandle handle{ m_registry.Insert(position, rotation, scale, color, meshName, uvScale) };
            m_engine.SendMsg(MsgObjectCreate{  handle, vve::ParentHandle{} });
			return handle;
    	};

	/**
	 * @brief Create an object with a mesh and texture
	 * @param name Name of the object
	 * @param parent Parent node handle
	 * @param meshName Name of the mesh
	 * @param textureName Name of the texture
	 * @param position Initial position
	 * @param rotation Initial rotation
	 * @param scale Initial scale
	 * @param uvScale UV scale
	 * @return Handle to the created object
	 */
	auto Engine::CreateObject(	Name name, ParentHandle parent, const MeshName& meshName, const TextureName& textureName,
								Position position, Rotation rotation, Scale scale, UVScale uvScale) -> ObjectHandle { 
            ObjectHandle handle{ m_registry.Insert(position, rotation, scale, meshName, textureName, uvScale) };
            m_engine.SendMsg(MsgObjectCreate{  handle, vve::ParentHandle{} });
			return handle;
    	};

	/**
	 * @brief Destroy an object
	 * @param handle Handle to the object to destroy
	 */
	void Engine::DestroyObject(ObjectHandle handle) {
		m_engine.SendMsg(MsgObjectDestroy{handle});
	};

	//-------------------------------------------------------------------------------------------------------------------

	/**
	 * @brief Create a point light
	 * @param name Name of the light
	 * @param parent Parent node handle
	 * @param light Point light properties
	 * @param position Initial position
	 * @param rotation Initial rotation
	 * @param scale Initial scale
	 * @return Handle to the created light
	 */
	auto Engine::CreatePointLight(Name name, ParentHandle parent, PointLight light, Position position, Rotation rotation, Scale scale) -> ObjectHandle { 
		ObjectHandle handle{ m_registry.Insert(name, parent, light, position, rotation, scale) };
		m_engine.SetParent(handle, parent);
		return handle;
	};

	/**
	 * @brief Create a directional light
	 * @param name Name of the light
	 * @param parent Parent node handle
	 * @param light Directional light properties
	 * @param position Initial position
	 * @param rotation Initial rotation
	 * @param scale Initial scale
	 * @return Handle to the created light
	 */
	auto Engine::CreateDirectionalLight(Name name, ParentHandle parent, DirectionalLight light,
							Position position, Rotation rotation, Scale scale) -> ObjectHandle { 
		ObjectHandle handle{ m_registry.Insert(name, parent, light, position, rotation, scale) };
		m_engine.SetParent(handle, parent);
		return handle;
	};

	/**
	 * @brief Create a spot light
	 * @param name Name of the light
	 * @param parent Parent node handle
	 * @param light Spot light properties
	 * @param position Initial position
	 * @param rotation Initial rotation
	 * @param scale Initial scale
	 * @return Handle to the created light
	 */
	auto Engine::CreateSpotLight(Name name, ParentHandle parent, SpotLight light,
							Position position, Rotation rotation, Scale scale) -> ObjectHandle { 
		ObjectHandle handle{ m_registry.Insert(name, parent, light, position, rotation, scale) };
		m_engine.SetParent(handle, parent);
		return handle;
	};

	/**
	 * @brief Create a camera
	 * @param name Name of the camera
	 * @param parent Parent node handle
	 * @param camera Camera properties
	 * @param position Initial position
	 * @param rotation Initial rotation
	 * @param scale Initial scale
	 * @return Handle to the created camera
	 */
	auto Engine::CreateCamera(Name name, ParentHandle parent, Camera camera, Position position, Rotation rotation, Scale scale) -> ObjectHandle { 
		ObjectHandle handle{ m_registry.Insert( name, parent, camera, position, rotation, scale )};
		m_engine.SetParent(handle, parent);
		return handle;
	};

	//-------------------------------------------------------------------------------------------------------------------

	/**
	 * @brief Get the local to parent transformation matrix
	 * @param handle Handle to the object
	 * @return Local to parent matrix
	 */
	auto Engine::GetLocalToParentMatrix(ObjectHandle handle) -> LocalToParentMatrix { 
		return m_registry.template Get<LocalToParentMatrix>(handle);
	};

	/**
	 * @brief Get the local to world transformation matrix
	 * @param handle Handle to the object
	 * @return Local to world matrix
	 */
	auto Engine::GetLocalToWorldMatrix(ObjectHandle handle) -> LocalToWorldMatrix { 
		return m_registry.template Get<LocalToWorldMatrix>(handle);
	};

	/**
	 * @brief Get the position of an object
	 * @param handle Handle to the object
	 * @return Position component
	 */
	auto Engine::GetPosition(ObjectHandle handle) -> Position {
		return m_registry.template Get<Position>(handle);
	};

	/**
	 * @brief Get the rotation of an object
	 * @param handle Handle to the object
	 * @return Rotation component
	 */
	auto Engine::GetRotation(ObjectHandle handle) -> Rotation {
		return m_registry.template Get<Rotation>(handle);
	};

	/**
	 * @brief Get the scale of an object
	 * @param handle Handle to the object
	 * @return Scale component
	 */
	auto Engine::GetScale(ObjectHandle handle) -> Scale {
		return m_registry.template Get<Scale>(handle);
	};

	/**
	 * @brief Get the UV scale of an object
	 * @param handle Handle to the object
	 * @return UV scale component
	 */
	auto Engine::GetUVScale(ObjectHandle handle) -> UVScale {
		return m_registry.template Get<UVScale>(handle);
	};

	/**
	 * @brief Set the local to parent transformation matrix
	 * @param handle Handle to the object
	 * @param matrix New local to parent matrix
	 */
	void Engine::SetLocalToParentMatrix(ObjectHandle handle, LocalToParentMatrix matrix) {
		return m_registry.Put(handle, matrix);
	};

	/**
	 * @brief Set the local to world transformation matrix
	 * @param handle Handle to the object
	 * @param matrix New local to world matrix
	 */
	void Engine::SetLocalToWorldMatrix(ObjectHandle handle, LocalToWorldMatrix matrix) {
		return m_registry.Put(handle, matrix);
	};

	/**
	 * @brief Set the position of an object
	 * @param handle Handle to the object
	 * @param position New position
	 */
	void Engine::SetPosition(ObjectHandle handle, Position position) {
		return m_registry.Put(handle, position);
	};

	/**
	 * @brief Set the rotation of an object
	 * @param handle Handle to the object
	 * @param rotation New rotation
	 */
	void Engine::SetRotation(ObjectHandle handle, Rotation rotation) {
		return m_registry.Put(handle, rotation);
	};

	/**
	 * @brief Set the scale of an object
	 * @param handle Handle to the object
	 * @param scale New scale
	 */
	void Engine::SetScale(ObjectHandle handle, Scale scale) {
		return m_registry.Put(handle, scale);
	};

	/**
	 * @brief Set the UV scale of an object
	 * @param handle Handle to the object
	 * @param uvScale New UV scale
	 */
	void Engine::SetUVScale(ObjectHandle handle, UVScale uvScale) {
		return m_registry.Put(handle, uvScale);
	};


	//-------------------------------------------------------------------------------------------------------------------

	/**
	 * @brief Destroy a mesh
	 * @param handle Handle to the mesh to destroy
	 */
	void Engine::DestroyMesh(MeshHandle handle) {
		m_engine.SendMsg(MsgMeshDestroy{handle});
	}

	/**
	 * @brief Destroy a texture
	 * @param handle Handle to the texture to destroy
	 */
	void Engine::DestroyTexture(TextureHandle handle) {
		m_engine.SendMsg(MsgTextureDestroy{handle});
	};


	//-------------------------------------------------------------------------------------------------------------------

	/**
	 * @brief Play a sound from a file
	 * @param filename Path to the sound file
	 * @param mode Sound play mode
	 * @param volume Sound volume
	 */
	void Engine::PlaySound(const Filename& filename, int mode, float volume) {
        m_engine.SendMsg(MsgPlaySound{ filename, mode, (int)volume });
	};

	/**
	 * @brief Set the global sound volume
	 * @param volume Volume level
	 */
	void Engine::SetVolume(float volume) {
		m_engine.SendMsg(MsgSetVolume{ (int)volume });

	};

};   // namespace vve

