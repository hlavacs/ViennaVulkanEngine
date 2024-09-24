#pragma once

#include <memory>
#include <unordered_map>
#include "VEWindowSDL.h"
#include "VERendererForward.h"
#include "VECS.h"


namespace vve {

	class VeEngine  {

	public:
		VeEngine(){};
		~VeEngine(){};

		void init(){
			m_initialized = true;
		};

		void shutdown(){
			m_running = false;
		};

		void loadLevel( const char* levelName ){
			// Load level
		};

		void createWindow( const char* windowName, int width, int height ){
			m_window = std::make_shared<VeWindowSDL>(windowName, width, height);
		};

		void createRenderer( const char* rendererName){
			// Create renderer
		};

		void createCamera( const char* cameraName ){
			// Create camera
		};

		void createSceneManager( const char* sceneManagerName ){
			// Create scene manager
		};

		void run(){
			if(!m_initialized){ init(); }
			m_running = true;
			while(m_running) {
				// Update
				// Render
			}
			shutdown();
		};

	private:
		bool m_initialized{false};
		bool m_running{false};
		vecs::Registry m_registry;
		std::unordered_map<std::string, vecs::Handle> m_nameMap;
		std::shared_ptr<VeWindow> m_window;
		std::shared_ptr<VeRenderer> m_renderer;

	};
};



