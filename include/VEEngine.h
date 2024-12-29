#pragma once

#include <memory>
#include <unordered_map>
#include "glm/glm.hpp"


namespace vve {

   	template<ArchitectureType ATYPE> class RendererVulkan;


	template<ArchitectureType ATYPE = ENGINETYPE_SEQUENTIAL>
	class Engine : public System<ATYPE> {

	public:

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
		auto GetWindow( std::string name ) -> Window<ATYPE>* { return (Window<ATYPE>*)GetSystem(name); }
		auto GetSceneMgr(std::string name) -> SceneManager<ATYPE>* { return (SceneManager<ATYPE>*)GetSystem(name); }
		void PrintCallbacks();

	protected:
		void OnInit(Message message);
		void OnInit2(Message message);
		void OnQuit(Message message);
		void LoadLevel( std::string levelName );
		void CreateWindow();
		void CreateRenderer();
		void CreateSystems();
		void CreateCamera();
		void CreateGUI();

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



