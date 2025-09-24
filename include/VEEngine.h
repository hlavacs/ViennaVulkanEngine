#pragma once

namespace vve {


	struct Camera; 

    enum class RendererType {
        RENDERER_TYPE_FORWARD,
        RENDERER_TYPE_DEFERRED,
        RENDERER_TYPE_RAYTRACING
    };

	class Engine : public System {

		static const uint32_t c_minimumVersion = VK_MAKE_VERSION(1, 1, 0); //for VMA

		#ifdef __APPLE__
		static const uint32_t c_maximumVersion = VK_MAKE_VERSION(1, 2, 0); //for VMA
		#else
		static const uint32_t c_maximumVersion = VK_MAKE_VERSION(1, 3, 0); //for VMA
		#endif

	public:

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
		const std::string m_rendererImguiName = "VVE Renderer Imgui";
		const std::string m_guiName = "VVE GUI";

		struct MessageCallback {
			System* 				 	  m_system;
			int 						  m_phase;	
			std::string			 	 	  m_messageName;
			std::function<bool(Message&)> m_callback;
		};

		Engine(std::string name, RendererType type = RendererType::RENDERER_TYPE_DEFERRED, uint32_t apiVersion = c_maximumVersion, bool debug=false);
		virtual ~Engine();
		void RegisterSystem( std::unique_ptr<System>&& system );
		void DeregisterSystem( System* system );
		void RegisterCallbacks( std::vector<MessageCallback> callbacks);
		void DeregisterCallbacks(System* system, std::string messageName);
		void Run();
		void Stop();
		void Init();
		void Step();
		void Quit();
		void SendMsg( Message message );
		void PrintCallbacks();
		auto GetRegistry() -> auto& { return m_registry; }
		auto GetState() { return EngineState{m_name, m_type, m_apiVersion, c_minimumVersion, c_maximumVersion, m_debug, m_initialized, m_running}; }
		auto GetSystem(std::string name) -> System* { return m_systems[name].get(); }
		auto GetShadowToggle() -> bool& { return m_shadowsEnabled; }
		auto IsShadowEnabled() const -> const bool { return m_shadowsEnabled; }

		//Get and set raw handles in the engine map
		auto GetHandle(std::string name) -> vecs::Handle;
		auto SetHandle(std::string name, vecs::Handle h) -> void;
		auto ContainsHandle(std::string name) -> bool;

		//Scene Nodes
		auto GetRootSceneNode() -> ObjectHandle;
		auto CreateSceneNode(Name name, ParentHandle parent, Position position = Position{vec3_t{0.0f}}, 
							Rotation rotation = Rotation{mat3_t{1.0f}}, Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		auto GetParent(ObjectHandle object) -> ParentHandle;
		void SetParent(ObjectHandle object, ParentHandle parent);

		//Loading from file
		void LoadScene( const Filename& name, aiPostProcessSteps flags = aiProcess_Triangulate);

		auto CreateObject(	Name name, ParentHandle parent, const MeshName& meshName, const TextureName& textureName, 
							Position position = Position{vec3_t{0.0f}}, Rotation rotation = Rotation{mat3_t{1.0f}}, 
							Scale scale = Scale{vec3_t{1.0f}}, UVScale uvScale = UVScale{vec2_t{1.0f}}) -> ObjectHandle;

		auto CreateObject(	Name name, ParentHandle parent, const MeshName& meshName, const vvh::Color color, 
							Position position = Position{vec3_t{0.0f}}, Rotation rotation = Rotation{mat3_t{1.0f}}, 
							Scale scale = Scale{vec3_t{1.0f}}, UVScale uvScale = UVScale{vec2_t{1.0f}}) -> ObjectHandle;

		auto CreateScene(	Name name, ParentHandle parent, const Filename& filename, aiPostProcessSteps flags,  
							Position position = Position{vec3_t{0.0f}}, Rotation rotation = Rotation{mat3_t{1.0f}}, 
							Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;
		
		void DestroyObject(ObjectHandle handle);

		//Create special scene nodes
		auto CreatePointLight(Name name, ParentHandle parent, PointLight light, Position position = Position{vec3_t{0.0f}}, 
							Rotation rotation = Rotation{mat3_t{1.0f}}, Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		auto CreateDirectionalLight(Name name, ParentHandle parent, DirectionalLight light, Position position = Position{vec3_t{0.0f}}, 
							Rotation rotation = Rotation{mat3_t{1.0f}}, Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		auto CreateSpotLight(Name name, ParentHandle parent, SpotLight light, Position position = Position{vec3_t{0.0f}}, 
							Rotation rotation = Rotation{mat3_t{1.0f}}, Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		auto CreateCamera(Name name, ParentHandle parent, Camera camera, Position position = Position{vec3_t{0.0f}}, 
						Rotation rotation = Rotation{mat3_t{1.0f}}, Scale scale = Scale{vec3_t{1.0f}}) -> ObjectHandle;

		//Get and set components
		auto GetLocalToParentMatrix(ObjectHandle handle) -> LocalToParentMatrix;
		auto GetLocalToWorldMatrix(ObjectHandle handle) -> LocalToWorldMatrix;
		auto GetPosition(ObjectHandle handle) -> Position;
		auto GetRotation(ObjectHandle handle) -> Rotation;
		auto GetScale(ObjectHandle handle) -> Scale;
		auto GetUVScale(ObjectHandle handle) -> UVScale;
		void SetLocalToParentMatrix(ObjectHandle handle, LocalToParentMatrix matrix);
		void SetLocalToWorldMatrix(ObjectHandle handle, LocalToWorldMatrix matrix);
		void SetPosition(ObjectHandle handle, Position position);
		void SetRotation(ObjectHandle handle, Rotation rotation);
		void SetScale(ObjectHandle handle, Scale scale);
		void SetUVScale(ObjectHandle handle, UVScale uvScale);

		//Create assets
		void DestroyMesh(MeshHandle);
		void DestroyTexture(TextureHandle);

		//sound
		void PlaySound(const Filename& filename, int mode, float volume=100);
		void SetVolume(float volume);

	protected:
		virtual void CreateWindows();
		virtual void CreateRenderer();
		virtual void CreateSystems();
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



