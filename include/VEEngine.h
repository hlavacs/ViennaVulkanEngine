#pragma once

#include <memory>
#include <unordered_map>
#include "glm/glm.hpp"

#include "VECS.h"
#include "VEInclude.h"

namespace vve {
   	template<ArchitectureType ATYPE> class RendererVulkan;

	template<ArchitectureType ATYPE = ENGINETYPE_SEQUENTIAL>
	class Engine : public System<ATYPE> {

		struct MessageCallback {
			System<ATYPE>* 				 m_system;
			int 						 m_phase;	
			MessageType 				 m_messageType;
			std::function<void(Message)> m_callback;
		};

	public:
		Engine(std::string name = "VVE Engine");
		virtual ~Engine();
		void RegisterSystem( std::vector<MessageCallback> callbacks);
		void DeregisterSystem( System<ATYPE>* system );
		void Run();
		void Stop();
		auto GetDebug() -> bool { return m_debug; }
		auto GetSceneMgr() -> SceneManager<ATYPE>* { return m_sceneManager.get(); }
		auto GetRegistry() -> auto& { return m_registry; }
		void SendMessage( Message message );
		auto GetSystem( std::string name ) -> System<ATYPE>*;
		auto GetWindow( std::string name ) -> Window<ATYPE>* { return (Window<ATYPE>*)GetSystem(name); }

	protected:
		virtual void OnInit(Message message);
		virtual void OnInit2(Message message);
		virtual void OnQuit(Message message);
		virtual void LoadLevel( std::string levelName );
		virtual void CreateWindow();
		virtual void CreateRenderer();
		virtual void CreateSystems();
		virtual void CreateCamera();

		std::unordered_map<std::string, System<ATYPE>*> m_systems{};

		bool m_debug{false};
		bool m_running{false};

		vecs::Registry<vecs::REGISTRYTYPE_SEQUENTIAL> m_registry;
		
		using PriorityMap = std::multimap<int, MessageCallback>;
		using MessageMap = std::unordered_map<MessageType, PriorityMap>;
		MessageMap m_messageMap{};

		std::vector<std::unique_ptr<Window<ATYPE>>> m_windows{};
		std::unique_ptr<SceneManager<ATYPE>> m_sceneManager;
	};

};  // namespace vve



