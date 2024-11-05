#pragma once

#include <memory>
#include <unordered_map>
#include "glm/glm.hpp"
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "VECS.h"


namespace vve {
   	template<ArchitectureType ATYPE>
    class RendererVulkan;

	template<ArchitectureType ATYPE = ArchitectureType::SEQUENTIAL>
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
		void RegisterSystem( System<ATYPE>* system, int phase, std::vector<MessageType> messageTypes );
		void RegisterSystem2( std::vector<MessageCallback> callbacks);
		void DeregisterSystem( System<ATYPE>* system );
		void DeregisterSystem2( System<ATYPE>* system );
		void Run();
		void Stop();
		auto GetDebug() -> bool { return m_debug; }
		auto GetWindows() -> std::vector<std::shared_ptr<Window<ATYPE>>>& { return m_windows; }
		auto GetSceneMgr() -> std::shared_ptr<SceneManager<ATYPE>> { return m_sceneManager; }
		auto GetRegistry() -> vecs::Registry<>& { return m_registry; }
		void SendMessage( Message message );
		void SendMessage2( Message message );
		auto GetSystem( std::string name ) -> System<ATYPE>* { return m_systems[name]; }	

	protected:
		virtual void OnInit(Message message) override;
		virtual void OnQuit(Message message) override;
		virtual void LoadLevel( std::string levelName );
		virtual void CreateWindow( const char* windowName, int width, int height );
		virtual void CreateRenderer( const char* rendererName);
		virtual void CreateCamera( const char* cameraName );
		virtual void CreateSceneManager( const char* sceneManagerName );

		std::unordered_map<std::string, System<ATYPE>*> m_systems{};

		bool m_debug{false};
		bool m_running{false};

		vecs::Registry<> m_registry;
		
		using PriorityMap = std::multimap<int, System<ATYPE>*>;
		using MessageMap = std::unordered_map<MessageType, PriorityMap>;
		MessageMap m_messageMap{};

		using PriorityMap2 = std::multimap<int, MessageCallback>;
		using MessageMap2 = std::unordered_map<MessageType, PriorityMap2>;
		MessageMap2 m_messageMap2{};

		std::vector<std::shared_ptr<Window<ATYPE>>> m_windows{};
		std::shared_ptr<SceneManager<ATYPE>> m_sceneManager;
	};

};  // namespace vve



