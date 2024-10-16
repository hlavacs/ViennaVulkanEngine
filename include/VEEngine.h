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

		struct VulkanState {
			VkAllocationCallbacks*   m_allocator = nullptr;
			VkInstance               m_instance = VK_NULL_HANDLE;
			VkDebugReportCallbackEXT m_debugReport = VK_NULL_HANDLE;
			VkPhysicalDevice         m_physicalDevice = VK_NULL_HANDLE;
			VkDevice                 m_device = VK_NULL_HANDLE;
			uint32_t                 m_queueFamily = (uint32_t)-1;		
			VkQueue                  m_queue = VK_NULL_HANDLE;
			VkPipelineCache          m_pipelineCache = VK_NULL_HANDLE;
		};

	public:
		VeEngine();
		virtual ~VeEngine();
		virtual void RegisterSystem( std::shared_ptr<VeSystem> system);
		virtual void Run();
		virtual void Stop();
		auto getState() -> const VulkanState& { return m_state; }	

	protected:
		void Init();
		void SetupVulkan();
		void Shutdown();

		virtual void LoadLevel( const char* levelName );
		virtual void CreateWindow( const char* windowName, int width, int height );
		virtual void CreateRenderer( const char* rendererName);
		virtual void CreateCamera( const char* cameraName );
		virtual void CreateSceneManager( const char* sceneManagerName );

		std::vector<const char*> m_instance_layers;
		std::vector<const char*> m_instance_extensions;
		std::vector<const char*> m_device_extensions{"VK_KHR_swapchain"};

		VulkanState m_state;

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



