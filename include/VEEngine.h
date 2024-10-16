#pragma once

#include <memory>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include "VHDevice.h"
#include "VEWindow.h"
#include "VERenderer.h"
#include "VESceneManager.h"
#include "VESystem.h"
#include "VECS.h"


namespace vve {

	class VeEngine  {

	public:
		VeEngine();
		virtual ~VeEngine();
		virtual void Init();
		virtual void RegisterSystem( std::shared_ptr<VeSystem> system);
		virtual void Shutdown();
		virtual void LoadLevel( const char* levelName );
		virtual void CreateWindow( const char* windowName, int width, int height );
		virtual void CreateRenderer( const char* rendererName);
		virtual void CreateCamera( const char* cameraName );
		virtual void CreateSceneManager( const char* sceneManagerName );
		virtual void Run();
		virtual void Stop();

		auto getAllocator() -> VkAllocationCallbacks* { return m_allocator; }
		auto getInstance() -> VkInstance { return m_instance; }
		auto getPhysicalDevice() -> VkPhysicalDevice { return m_physicalDevice; }
		auto getDevice() -> VkDevice { return m_device; }
		auto getQueueFamily() -> uint32_t { return m_queueFamily; }
		auto getQueue() -> VkQueue { return m_queue; }
		auto getPipelineCache() -> VkPipelineCache { return m_pipelineCache; }	

	private:
		void SetupVulkan();

		std::vector<const char*> m_instance_layers;
		std::vector<const char*> m_instance_extensions;
		std::vector<const char*> m_device_extensions{"VK_KHR_swapchain"};

		VkAllocationCallbacks*   m_allocator = nullptr;
		VkInstance               m_instance = VK_NULL_HANDLE;
		VkDebugReportCallbackEXT m_debugReport = VK_NULL_HANDLE;
		VkPhysicalDevice         m_physicalDevice = VK_NULL_HANDLE;
		VkDevice                 m_device = VK_NULL_HANDLE;
		uint32_t                 m_queueFamily = (uint32_t)-1;		
		VkQueue                  m_queue = VK_NULL_HANDLE;
		VkPipelineCache          m_pipelineCache = VK_NULL_HANDLE;

		bool m_debug{false};
		bool m_initialized{false};
		bool m_running{false};
		vecs::Registry<> m_registry;
		std::unordered_map<std::string, vecs::Handle> m_nameMap{};
		std::unique_ptr<VeWindow> m_window{};
		std::unique_ptr<VeRenderer> m_renderer{};
		std::unique_ptr<VeSceneManager> m_sceneManager{};
	};
};



