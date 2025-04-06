#pragma once

namespace vve {



	class Engine : public System {

		static const uint32_t c_minimumVersion = VK_MAKE_VERSION(1, 1, 0); //for VMA
		static const uint32_t c_maximumVersion = VK_MAKE_VERSION(1, 3, 0); //for VMA

	public:

		struct EngineState {
			const std::string& name;
			uint32_t& apiVersion;
			uint32_t minimumVersion = c_minimumVersion;
			uint32_t maximumVersion = c_maximumVersion;
			bool debug;		
			bool initialized;
			bool running;	
		};

		const std::string m_windowName = "VVE Window";
		const std::string m_sceneManagerName = "VVE SceneManager";
		const std::string m_assetManagerName = "VVE AssetManager";
		const std::string m_soundManagerName = "VVE SoundManager";
		const std::string m_rendererName = "VVE Renderer";
		const std::string m_rendererVulkanaName = "VVE Renderer Vulkan";
		const std::string m_rendererForwardName = "VVE Renderer Forward";
		const std::string m_rendererImguiName = "VVE Renderer Imgui";
		const std::string m_guiName = "VVE GUI";

		struct MessageCallback {
			System* 				 	  m_system;
			int 						  m_phase;	
			std::string			 	 	  m_messageName;
			std::function<bool(Message&)> m_callback;
		};

		Engine(std::string name, uint32_t apiVersion = c_maximumVersion, bool debug=false);
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
		auto GetHandle(std::string name) -> vecs::Handle;
		auto SetHandle(std::string name, vecs::Handle h) -> void;
		auto ContainsHandle(std::string name) -> bool;
		auto GetRegistry() -> auto& { return m_registry; }
		auto GetState() { return EngineState{m_name, m_apiVersion, c_minimumVersion, c_maximumVersion, m_debug, m_initialized, m_running}; }
		auto GetSystem(std::string name) -> System* { return m_systems[name].get(); }

	protected:
		virtual void CreateWindows();
		virtual void CreateRenderer();
		virtual void CreateSystems();
		virtual void CreateGUI();

		std::unordered_map<std::string, std::unique_ptr<System>> m_systems{};

		uint32_t m_apiVersion;
		bool m_debug{false};
		bool m_initialized{false};
		bool m_running{false};

		std::chrono::time_point<std::chrono::high_resolution_clock> m_last;

		vecs::Registry m_registry; //VECS lives here
		std::unordered_map<std::string, vecs::Handle> m_handleMap; //from string to handle

		using PriorityMap = std::multimap<int, MessageCallback>;
		using MessageMap = std::map<size_t, PriorityMap>;
		MessageMap m_messageMap{};

		std::map<size_t, std::string> m_msgTypeMap;
	};

};  // namespace vve



