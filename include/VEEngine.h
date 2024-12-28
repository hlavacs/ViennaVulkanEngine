#pragma once

#include <memory>
#include <unordered_map>
#include "glm/glm.hpp"


namespace vve {

   	template<ArchitectureType ATYPE> class RendererVulkan;


	template<ArchitectureType ATYPE = ENGINETYPE_SEQUENTIAL>
	class Engine : public System<ATYPE> {

	public:
		const std::string m_mainWindowName = "VVE MainWindow";
		const std::string m_sceneManagerName = "VVE SceneManager";

		struct MessageCallback {
			System<ATYPE>* 				 m_system;
			int 						 m_phase;	
			std::string			 	 	 m_messageName;
			std::function<void(Message)> m_callback;
		};

		Engine(std::string name);
		virtual ~Engine();
		void RegisterSystem( std::unique_ptr<System<ATYPE>>&& system );
		void RegisterCallback( std::vector<MessageCallback> callbacks);
		void DeregisterSystem( System<ATYPE>* system );
		void Run();
		void Stop();
		void Init();
		void Step();
		void Quit();
		auto GetDebug() -> bool { return m_debug; }
		auto GetRegistry() -> auto& { return m_registry; }
		void SendMessage( Message message );
		auto GetSystem( std::string name ) -> System<ATYPE>*;
		auto GetMainWindowName() -> std::string { return m_mainWindowName; }
		auto GetMainWindow() -> Window<ATYPE>* { return GetWindow(GetMainWindowName()); }
		auto GetWindow( std::string name ) -> Window<ATYPE>* { return (Window<ATYPE>*)GetSystem(name); }
		auto GetSceneMgr(std::string name) -> SceneManager<ATYPE>* { return (SceneManager<ATYPE>*)GetSystem(name); }
		void PrintCallbacks();

	protected:
		virtual void OnInit(Message message);
		virtual void OnInit2(Message message);
		virtual void OnQuit(Message message);
		virtual void LoadLevel( std::string levelName );
		virtual void CreateWindow();
		virtual void CreateRenderer();
		virtual void CreateSystems();
		virtual void CreateCamera();
		virtual void CreateGUI();

		std::unordered_map<std::string, std::unique_ptr<System<ATYPE>>> m_systems{};

		bool m_debug{false};
		bool m_initialized{false};
		bool m_running{false};
		std::chrono::time_point<std::chrono::high_resolution_clock> m_last;

		vecs::Registry<ATYPE> m_registry;
		
		using PriorityMap = std::multimap<int, MessageCallback>;
		using MessageMap = std::map<size_t, PriorityMap>;
		MessageMap m_messageMap{};

		std::map<size_t, std::string> m_msgTypeMap;
	};

};  // namespace vve



