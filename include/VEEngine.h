#pragma once

#include <memory>
#include <unordered_map>
#include "VEWindow.h"
#include "VERenderer.h"
#include "VECS.h"


namespace vve {

	class VeEngine  {

	public:
		VeEngine();
		virtual ~VeEngine();
		virtual void init();
		virtual void shutdown();
		virtual void loadLevel( const char* levelName );
		virtual void createWindow( const char* windowName, int width, int height );
		virtual void createRenderer( const char* rendererName);
		virtual void createCamera( const char* cameraName );
		virtual void createSceneManager( const char* sceneManagerName );
		virtual void run();
		virtual void stop();

	private:
		bool m_initialized{false};
		bool m_running{false};
		vecs::Registry<> m_registry;
		std::unordered_map<std::string, vecs::Handle> m_nameMap{};
		std::shared_ptr<VeWindow> m_window{};
		std::shared_ptr<VeRenderer> m_renderer{};

	};
};



