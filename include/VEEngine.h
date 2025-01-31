#pragma once

#include <memory>
#include <unordered_map>
#include "glm/glm.hpp"


namespace vve {


	class Engine : public System {

	public:

		const std::string m_windowName = "VVE Window";
		const std::string m_sceneManagerName = "VVE SceneManager";
		const std::string m_assetManagerName = "VVE AssetManager";
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

		Engine(std::string name, bool debug=false);
		virtual ~Engine();
		void RegisterSystem( std::unique_ptr<System>&& system );
		void RegisterCallback( std::vector<MessageCallback> callbacks);
		void DeregisterSystem( System* system );
		void Run();
		void Stop();
		void Init();
		void Step();
		void Quit();
		auto GetDebug() -> bool { return m_debug; }
		auto GetRegistry() -> auto& { return m_registry; }
		void SendMessage( Message message );
		auto GetSystem( std::string name ) -> System*;
		auto GetWindow( std::string name ) -> Window* { return (Window*)GetSystem(name); }
		auto GetRenderer( std::string name ) -> Renderer* { return (Renderer*)GetSystem(name); }
		auto GetSceneMgr(std::string name) -> SceneManager* { return (SceneManager*)GetSystem(name); }
		void PrintCallbacks();

	protected:
		virtual void CreateWindow();
		virtual void CreateRenderer();
		virtual void CreateSystems();
		virtual void CreateGUI();

		std::unordered_map<std::string, std::unique_ptr<System>> m_systems{};

		bool m_debug{false};
		bool m_initialized{false};
		bool m_running{false};
		std::chrono::time_point<std::chrono::high_resolution_clock> m_last;

		vecs::Registry<VVE_ARCHITECTURE_TYPE> m_registry;
		
		using PriorityMap = std::multimap<int, MessageCallback>;
		using MessageMap = std::map<size_t, PriorityMap>;
		MessageMap m_messageMap{};

		std::map<size_t, std::string> m_msgTypeMap;
	};

};  // namespace vve



