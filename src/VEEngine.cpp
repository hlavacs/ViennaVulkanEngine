#include "VEEngine.h"

using namespace vve;

VeEngine::VeEngine(){};

VeEngine::~VeEngine(){};

void VeEngine::init(){
	m_initialized = true;
};

void VeEngine::shutdown(){
	m_running = false;
};

void VeEngine::loadLevel( const char* levelName ){
	// Load level
};

void VeEngine::createWindow( const char* windowName, int width, int height ){
	m_window = std::make_shared<VeWindowSDL>(windowName, width, height);
};

void VeEngine::createRenderer( const char* rendererName){
	// Create renderer
};

void VeEngine::createCamera( const char* cameraName ){
	// Create camera
};

void VeEngine::createSceneManager( const char* sceneManagerName ){
	// Create scene manager
};

void VeEngine::run(){
	if(!m_initialized){ init(); }
	m_running = true;
	while(m_running) {
		// Update
		// Render
	}
	shutdown();
};








