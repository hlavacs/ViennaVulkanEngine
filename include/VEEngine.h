#pragma once

namespace vve {


	struct Camera;

    /**
     * @enum RendererType
     * @brief Specifies the type of renderer to use for the engine.
     */
    enum class RendererType {
        RENDERER_TYPE_FORWARD,
        RENDERER_TYPE_DEFERRED,
        RENDERER_TYPE_RAYTRACING
    };

	/**
	 * @class Engine
	 * @brief Core engine class that manages all systems, rendering, and scene management.
	 */
	class Engine : public System {

		static const uint32_t c_minimumVersion = VK_MAKE_VERSION(1, 1, 0); //for VMA

		#ifdef __APPLE__
		static const uint32_t c_maximumVersion = VK_MAKE_VERSION(1, 2, 0); //for VMA
		#else
		static const uint32_t c_maximumVersion = VK_MAKE_VERSION(1, 3, 0); //for VMA
		#endif

	public:

		/**
		 * @struct EngineState
		 * @brief Holds the current state and configuration of the engine.
		 */
		struct EngineState {
			const std::string& m_name;
			RendererType m_type;
			uint32_t& 	m_apiVersion;
			uint32_t	m_minimumVersion = c_minimumVersion;
			uint32_t 	m_maximumVersion = c_maximumVersion;
			bool 		m_debug;
			bool 		m_initialized;
			bool 		m_running;
		};

		const std::string m_windowName = "VVE Window";
		const std::string m_sceneManagerName = "VVE SceneManager";
		const std::string m_assetManagerName = "VVE AssetManager";
		const std::string m_soundManagerName = "VVE SoundManager";
		const std::string m_rendererName = "VVE Renderer";
		const std::string m_rendererVulkanName = "VVE Renderer Vulkan";
		const std::string m_rendererForwardName = "VVE Renderer Forward";
		const std::string m_rendererDeferredName = "VVE Renderer Deferred";
		const std::string m_rendererRaytracingName = "VVE Renderer Raytracing";
		const std::string m_rendererImguiName = "VVE Renderer Imgui";
		const std::string m_guiName = "VVE GUI";

		/**
		 * @struct MessageCallback
		 * @brief Encapsulates a callback function for message handling in the engine.
		 */
		struct MessageCallback {
			System* 				 	  m_system;
			int 						  m_phase;
			std::string			 	 	  m_messageName;
			std::function<bool(Message&)> m_callback;
		};

		/**
		 * @brief Constructs the Engine with specified configuration.
		 * @param name Name of the engine instance.
		 * @param type Renderer type to use (default: RENDERER_TYPE_DEFERRED).
		 * @param apiVersion Vulkan API version (default: c_maximumVersion).
		 * @param debug Enable debug mode (default: false).
		 */
		Engine(std::string name, RendererType type = RendererType::RENDERER_TYPE_DEFERRED, uint32_t apiVersion = c_maximumVersion, bool debug=false);
		/**
		 * @brief Destructor for the Engine.
		 */
		virtual ~Engine();
		/**
		 * @brief Registers a system with the engine.
		 * @param system Unique pointer to the system to register.
		 */
		void RegisterSystem( std::unique_ptr<System>&& system );
		/**
		 * @brief Deregisters a system from the engine.
		 * @param system Pointer to the system to deregister.
		 */
		void DeregisterSystem( System* system );
		/**
		 * @brief Registers message callbacks with the engine.
		 * @param callbacks Vector of MessageCallback objects to register.
		 */
		void RegisterCallbacks( std::vector<MessageCallback> callbacks);
		/**
		 * @brief Deregisters callbacks for a specific system and message.
		 * @param system Pointer to the system whose callbacks to deregister.
		 * @param messageName Name of the message type.
		 */
		void DeregisterCallbacks(System* system, std::string messageName);
		/**
		 * @brief Starts the engine's main loop.
		 */
		void Run();
		/**
		 * @brief Stops the engine's main loop.
		 */
		void Stop();
		/**
		 * @brief Initializes the engine and all registered systems.
		 */
		void Init();
		/**
		 * @brief Executes a single step of the engine loop.
		 */
		void Step();
		/**
		 * @brief Quits the engine and cleans up resources.
		 */
		void Quit();
		/**
		 * @brief Sends a message to registered callbacks.
		 * @param message The message to send.
		 */
		void SendMsg( Message message );
		/**
		 * @brief Prints all registered callbacks (for debugging).
		 */
		void PrintCallbacks();
		/**
		 * @brief Gets the VECS registry.
		 * @return Reference to the registry.
		 */
		auto GetRegistry() -> auto& { return m_registry; }
		/**
		 * @brief Gets the current engine state.
		 * @return EngineState struct with current state information.
		 */
		auto GetState() { return EngineState{m_name, m_type, m_apiVersion, c_minimumVersion, c_maximumVersion, m_debug, m_initialized, m_running}; }
		/**
		 * @brief Gets a system by name.
		 * @param name Name of the system to retrieve.
		 * @return Pointer to the system, or nullptr if not found.
		 */
		auto GetSystem(std::string name) -> System* { return m_systems[name].get(); }
		/**
		 * @brief Gets a reference to the shadow toggle.
		 * @return Reference to the shadow enabled boolean.
		 */
		auto GetShadowToggle() -> bool& { return m_shadowsEnabled; }
		/**
		 * @brief Checks if shadows are enabled.
		 * @return True if shadows are enabled, false otherwise.
		 */
		auto IsShadowEnabled() const -> const bool { return m_shadowsEnabled; }

		//Get and set raw handles in the engine map
		/**
		 * @brief Gets a handle by name from the engine map.
		 * @param name Name of the handle to retrieve.
		 * @return The handle associated with the name.
		 */
		auto GetHandle(std::string name) -> vecs::Handle;
		/**
		 * @brief Sets a handle by name in the engine map.
		 * @param name Name to associate with the handle.
		 * @param h Handle to store.
		 */
		auto SetHandle(std::string name, vecs::Handle h) -> void;
		/**
		 * @brief Checks if a handle exists in the engine map.
		 * @param name Name of the handle to check.
		 * @return True if the handle exists, false otherwise.
		 */
		auto ContainsHandle(std::string name) -> bool;

		//Scene Nodes
		/**
		 * @brief Gets the root scene node.
		 * @return Handle to the root scene node.
		 */
		auto GetRootSceneNode() -> ObjectHandle;
		/**
		 * @brief Creates a new scene node.
		 * @param name Name of the scene node.
		 * @param parent Parent handle for the node.
		 * @param position Position of the node (default: origin).
		 * @param rotation Rotation of the node (default: identity).
		 * @param scale Scale of the node (default: unit scale).
		 * @return Handle to the created scene node.
		 */
		auto CreateSceneNode(Name name, ParentHandle parent, Position position = Position{vec3_t{0.0f}},
							Rotation rotation = Rotation{mat3_t{1.0f}}, Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		/**
		 * @brief Gets the parent of an object.
		 * @param object Handle to the object.
		 * @return Handle to the parent.
		 */
		auto GetParent(ObjectHandle object) -> ParentHandle;
		/**
		 * @brief Sets the parent of an object.
		 * @param object Handle to the object.
		 * @param parent Handle to the new parent.
		 */
		void SetParent(ObjectHandle object, ParentHandle parent);

		//Loading from file
		/**
		 * @brief Loads a scene from a file.
		 * @param name Filename of the scene to load.
		 * @param flags Assimp post-processing flags (default: aiProcess_Triangulate).
		 */
		void LoadScene( const Filename& name, aiPostProcessSteps flags = aiProcess_Triangulate);

		/**
		 * @brief Creates an object with a mesh and texture.
		 * @param name Name of the object.
		 * @param parent Parent handle for the object.
		 * @param meshName Name of the mesh to use.
		 * @param textureName Name of the texture to use.
		 * @param position Position of the object (default: origin).
		 * @param rotation Rotation of the object (default: identity).
		 * @param scale Scale of the object (default: unit scale).
		 * @param uvScale UV scale of the object (default: unit scale).
		 * @return Handle to the created object.
		 */
		auto CreateObject(	Name name, ParentHandle parent, const MeshName& meshName, const TextureName& textureName,
							Position position = Position{vec3_t{0.0f}}, Rotation rotation = Rotation{mat3_t{1.0f}},
							Scale scale = Scale{vec3_t{1.0f}}, UVScale uvScale = UVScale{vec2_t{1.0f}}) -> ObjectHandle;

		auto CreateObject(Name name, ParentHandle parent, const MeshName& meshName, const MaterialName& materialName, const TextureName& textureName,
			Position position = Position{ vec3_t{0.0f} }, Rotation rotation = Rotation{ mat3_t{1.0f} },
			Scale scale = Scale{ vec3_t{1.0f} }, UVScale uvScale = UVScale{ vec2_t{1.0f} }) -> ObjectHandle;

		/**
		 * @brief Creates an object with a mesh and color.
		 * @param name Name of the object.
		 * @param parent Parent handle for the object.
		 * @param meshName Name of the mesh to use.
		 * @param color Color of the object.
		 * @param position Position of the object (default: origin).
		 * @param rotation Rotation of the object (default: identity).
		 * @param scale Scale of the object (default: unit scale).
		 * @param uvScale UV scale of the object (default: unit scale).
		 * @return Handle to the created object.
		 */
		auto CreateObject(	Name name, ParentHandle parent, const MeshName& meshName, const vvh::Color color,
							Position position = Position{vec3_t{0.0f}}, Rotation rotation = Rotation{mat3_t{1.0f}},
							Scale scale = Scale{vec3_t{1.0f}}, UVScale uvScale = UVScale{vec2_t{1.0f}}) -> ObjectHandle;

		/**
		 * @brief Creates a scene from a file.
		 * @param name Name of the scene.
		 * @param parent Parent handle for the scene.
		 * @param filename Filename of the scene to load.
		 * @param flags Assimp post-processing flags.
		 * @param position Position of the scene (default: origin).
		 * @param rotation Rotation of the scene (default: identity).
		 * @param scale Scale of the scene (default: unit scale).
		 * @return Handle to the created scene.
		 */
		auto CreateScene(	Name name, ParentHandle parent, const Filename& filename, aiPostProcessSteps flags,
							Position position = Position{vec3_t{0.0f}}, Rotation rotation = Rotation{mat3_t{1.0f}},
							Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		/**
		 * @brief Destroys an object.
		 * @param handle Handle to the object to destroy.
		 */
		void DestroyObject(ObjectHandle handle);

		//Create special scene nodes
		/**
		 * @brief Creates a point light.
		 * @param name Name of the point light.
		 * @param parent Parent handle for the light.
		 * @param light Point light properties.
		 * @param position Position of the light (default: origin).
		 * @param rotation Rotation of the light (default: identity).
		 * @param scale Scale of the light (default: unit scale).
		 * @return Handle to the created point light.
		 */
		auto CreatePointLight(Name name, ParentHandle parent, PointLight light, Position position = Position{vec3_t{0.0f}},
							Rotation rotation = Rotation{mat3_t{1.0f}}, Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		/**
		 * @brief Creates a directional light.
		 * @param name Name of the directional light.
		 * @param parent Parent handle for the light.
		 * @param light Directional light properties.
		 * @param position Position of the light (default: origin).
		 * @param rotation Rotation of the light (default: identity).
		 * @param scale Scale of the light (default: unit scale).
		 * @return Handle to the created directional light.
		 */
		auto CreateDirectionalLight(Name name, ParentHandle parent, DirectionalLight light, Position position = Position{vec3_t{0.0f}},
							Rotation rotation = Rotation{mat3_t{1.0f}}, Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		/**
		 * @brief Creates a spot light.
		 * @param name Name of the spot light.
		 * @param parent Parent handle for the light.
		 * @param light Spot light properties.
		 * @param position Position of the light (default: origin).
		 * @param rotation Rotation of the light (default: identity).
		 * @param scale Scale of the light (default: unit scale).
		 * @return Handle to the created spot light.
		 */
		auto CreateSpotLight(Name name, ParentHandle parent, SpotLight light, Position position = Position{vec3_t{0.0f}},
							Rotation rotation = Rotation{mat3_t{1.0f}}, Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		/**
		 * @brief Creates a camera.
		 * @param name Name of the camera.
		 * @param parent Parent handle for the camera.
		 * @param camera Camera properties.
		 * @param position Position of the camera (default: origin).
		 * @param rotation Rotation of the camera (default: identity).
		 * @param scale Scale of the camera (default: unit scale).
		 * @return Handle to the created camera.
		 */
		auto CreateCamera(Name name, ParentHandle parent, Camera camera, Position position = Position{vec3_t{0.0f}},
						Rotation rotation = Rotation{mat3_t{1.0f}}, Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		//Get and set components
		/**
		 * @brief Gets the local-to-parent transformation matrix of an object.
		 * @param handle Handle to the object.
		 * @return Local-to-parent transformation matrix.
		 */
		auto GetLocalToParentMatrix(ObjectHandle handle) -> LocalToParentMatrix;
		/**
		 * @brief Gets the local-to-world transformation matrix of an object.
		 * @param handle Handle to the object.
		 * @return Local-to-world transformation matrix.
		 */
		auto GetLocalToWorldMatrix(ObjectHandle handle) -> LocalToWorldMatrix;
		/**
		 * @brief Gets the position of an object.
		 * @param handle Handle to the object.
		 * @return Position of the object.
		 */
		auto GetPosition(ObjectHandle handle) -> Position;
		/**
		 * @brief Gets the rotation of an object.
		 * @param handle Handle to the object.
		 * @return Rotation of the object.
		 */
		auto GetRotation(ObjectHandle handle) -> Rotation;
		/**
		 * @brief Gets the scale of an object.
		 * @param handle Handle to the object.
		 * @return Scale of the object.
		 */
		auto GetScale(ObjectHandle handle) -> Scale;
		/**
		 * @brief Gets the UV scale of an object.
		 * @param handle Handle to the object.
		 * @return UV scale of the object.
		 */
		auto GetUVScale(ObjectHandle handle) -> UVScale;
		/**
		 * @brief Sets the local-to-parent transformation matrix of an object.
		 * @param handle Handle to the object.
		 * @param matrix Local-to-parent transformation matrix to set.
		 */
		void SetLocalToParentMatrix(ObjectHandle handle, LocalToParentMatrix matrix);
		/**
		 * @brief Sets the local-to-world transformation matrix of an object.
		 * @param handle Handle to the object.
		 * @param matrix Local-to-world transformation matrix to set.
		 */
		void SetLocalToWorldMatrix(ObjectHandle handle, LocalToWorldMatrix matrix);
		/**
		 * @brief Sets the position of an object.
		 * @param handle Handle to the object.
		 * @param position Position to set.
		 */
		void SetPosition(ObjectHandle handle, Position position);
		/**
		 * @brief Sets the rotation of an object.
		 * @param handle Handle to the object.
		 * @param rotation Rotation to set.
		 */
		void SetRotation(ObjectHandle handle, Rotation rotation);
		/**
		 * @brief Sets the scale of an object.
		 * @param handle Handle to the object.
		 * @param scale Scale to set.
		 */
		void SetScale(ObjectHandle handle, Scale scale);
		/**
		 * @brief Sets the UV scale of an object.
		 * @param handle Handle to the object.
		 * @param uvScale UV scale to set.
		 */
		void SetUVScale(ObjectHandle handle, UVScale uvScale);

		//Create assets
		/**
		 * @brief Destroys a mesh.
		 * @param meshHandle Handle to the mesh to destroy.
		 */
		void DestroyMesh(MeshHandle);
		/**
		 * @brief Destroys a texture.
		 * @param textureHandle Handle to the texture to destroy.
		 */
		void DestroyTexture(TextureHandle);

		//sound
		/**
		 * @brief Plays a sound from a file.
		 * @param filename Filename of the sound to play.
		 * @param mode Playback mode.
		 * @param volume Volume level (default: 100).
		 */
		void PlaySound(const Filename& filename, int mode, float volume=100);
		/**
		 * @brief Sets the global sound volume.
		 * @param volume Volume level to set.
		 */
		void SetVolume(float volume);

	protected:
		/**
		 * @brief Creates the window system (virtual, can be overridden).
		 */
		virtual void CreateWindows();
		/**
		 * @brief Creates the renderer system (virtual, can be overridden).
		 */
		virtual void CreateRenderer();
		/**
		 * @brief Creates engine systems (virtual, can be overridden).
		 */
		virtual void CreateSystems();
		/**
		 * @brief Creates the GUI system (virtual, can be overridden).
		 */
		virtual void CreateGUI();

		std::unordered_map<std::string, std::unique_ptr<System>> m_systems{};

		RendererType m_type;
		uint32_t m_apiVersion;
		bool m_debug{false};
		bool m_initialized{false};
		bool m_running{false};

		bool m_shadowsEnabled{ true };

		std::chrono::time_point<std::chrono::high_resolution_clock> m_last;

		vecs::Registry m_registry; //VECS lives here
		std::unordered_map<std::string, vecs::Handle> m_handleMap; //from string to handle

		using PriorityMap = std::multimap<int, MessageCallback>;
		using MessageMap = std::map<size_t, PriorityMap>;
		MessageMap m_messageMap{};

		std::map<size_t, std::string> m_msgTypeMap;

	};

};  // namespace vve



