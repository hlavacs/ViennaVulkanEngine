#include "VEEngine.h"
#include "VEWindowSDL.h"

using namespace vve;

VeEngine::VeEngine() {
#ifdef DEBUG
	m_debug = true;
#endif
};

VeEngine::~VeEngine(){};

void VeEngine::Init(){
	if(m_initialized) return;
	CreateWindow("Vulkan Engine", 800, 600);
	SetupVulkan();
	CreateRenderer("Forward");
	CreateCamera("Main Camera");
	CreateSceneManager("");
	LoadLevel("");
	m_initialized = true;
};


void VeEngine::SetupVulkan() {
	std::vector<const char*> instance_layers;
	std::vector<const char*> instance_extensions;
	std::vector<const char*> device_extensions{"VK_KHR_swapchain"};
	
	if(m_debug) {
        instance_layers.push_back("VK_LAYER_KHRONOS_validation");
        instance_extensions.push_back("VK_EXT_debug_report");
	}

	vh::SetUpInstance(instance_layers, instance_extensions, m_allocator, &m_instance);
	if(m_debug) vh::SetupDebugReport(m_instance, m_allocator, &m_debugReport);
	vh::SetupPhysicalDevice(m_instance, device_extensions, &m_physicalDevice);
	vh::SetupGraphicsQueueFamily(m_physicalDevice, &m_queueFamily);
    vh::SetupDevice( m_physicalDevice, nullptr, device_extensions, m_queueFamily, &m_device);
	vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
	vh::SetupDescriptorPool(m_device, &m_descriptorPool);
	vh::SetupSurface( m_device, m_descriptorPool, &m_surface);
};

void VeEngine::RegisterSystem( std::shared_ptr<VeSystem> system) {
	// Register system
}


void VeEngine::LoadLevel( const char* levelName ){
	// Load level
};

void VeEngine::CreateWindow( const char* windowName, int width, int height ){
	m_window = std::make_unique<VeWindowSDL>(m_instance, windowName, width, height);
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
	vkDestroyDescriptorPool(m_device, m_descriptorPool, m_allocator);

    auto PFN_DestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
    PFN_DestroyDebugReportCallbackEXT(m_instance, m_debugReport, m_allocator);

    vkDestroyDevice(m_device, m_allocator);
    vkDestroyInstance(m_instance, m_allocator);
}



