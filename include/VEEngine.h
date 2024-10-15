#pragma once

#include <memory>
#include <unordered_map>
#include "VEWindow.h"
#include "VERenderer.h"
#include "VESceneManager.h"
#include "VECS.h"


namespace vve {

	class VeEngine  {

	public:
		VeEngine();
		virtual ~VeEngine();
		virtual void Init();
		virtual void RegisterSystem();
		virtual void Shutdown();
		virtual void LoadLevel( const char* levelName );
		virtual void CreateWindow( const char* windowName, int width, int height );
		virtual void CreateRenderer( const char* rendererName);
		virtual void CreateCamera( const char* cameraName );
		virtual void CreateSceneManager( const char* sceneManagerName );
		virtual void Run();
		virtual void Stop();

	private:
		bool m_initialized{false};
		bool m_running{false};
		vecs::Registry<> m_registry;
		std::unordered_map<std::string, vecs::Handle> m_nameMap{};
		std::unique_ptr<VeWindow> m_window{};
		std::unique_ptr<VeRenderer> m_renderer{};
		std::unique_ptr<VeSceneManager> m_sceneManager{};
	};
};



