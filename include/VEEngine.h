#pragma once

#include <memory>
#include <unordered_map>
#include "VEWindowSDL.h"
#include "VERendererForward.h"
#include "VECS.h"


namespace vve {

	class VeEngine  {

	public:
		VeEngine();
		~VeEngine();
		void init();
		void shutdown();
		void loadLevel( const char* levelName );
		void createWindow( const char* windowName, int width, int height );
		void createRenderer( const char* rendererName);
		void createCamera( const char* cameraName );
		void createSceneManager( const char* sceneManagerName );
		void run();

	private:
		bool m_initialized{false};
		bool m_running{false};
		//vecs::Registry m_registry;
		std::unordered_map<std::string, vecs::Handle> m_nameMap{};
		std::shared_ptr<VeWindow> m_window{};
		std::shared_ptr<VeRenderer> m_renderer{};

	};
};



