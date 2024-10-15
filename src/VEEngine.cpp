#include "VEEngine.h"
#include "VEWindowSDL.h"

using namespace vve;

VeEngine::VeEngine(){};

VeEngine::~VeEngine(){};

void VeEngine::Init(){
	if(m_initialized) return;
	CreateWindow("Vulkan Engine", 800, 600);
	vh::SetupVulkan();
	CreateRenderer("Forward");
	CreateCamera("Main Camera");
	CreateSceneManager("");
	LoadLevel("");
	m_initialized = true;
};


void VeEngine::LoadLevel( const char* levelName ){
	// Load level
};

void VeEngine::CreateWindow( const char* windowName, int width, int height ){
	m_window = std::make_unique<VeWindowSDL>(windowName, width, height);
};

void VeEngine::CreateRenderer( const char* rendererName){
	// Create renderer
};

void VeEngine::CreateCamera( const char* cameraName ){
	// Create camera
};

void VeEngine::CreateSceneManager( const char* sceneManagerName ){
	// Create scene manager
};

void VeEngine::Run(){
	Init();
	m_running = true;
	while(m_running) { //call stop to stop the engine
		// Window
		// Update
		// Render
		// GUI
	}
	Shutdown();
};

void VeEngine::Stop(){
	m_running = false;
};


void VeEngine::Shutdown(){
};

