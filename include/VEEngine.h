#pragma once

#include <memory>
#include <unordered_map>
#include <vulkan/vulkan.h>
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

		VkAllocationCallbacks*   g_Allocator = nullptr;
		VkInstance               g_Instance = VK_NULL_HANDLE;
		VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice                 g_Device = VK_NULL_HANDLE;
		uint32_t                 g_QueueFamily = (uint32_t)-1;
		VkQueue                  g_Queue = VK_NULL_HANDLE;
		VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
		VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
		VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

		uint32_t                 g_MinImageCount = 2;
		bool                     g_SwapChainRebuild = false;

		bool m_initialized{false};
		bool m_running{false};
		vecs::Registry<> m_registry;
		std::unordered_map<std::string, vecs::Handle> m_nameMap{};
		std::unique_ptr<VeWindow> m_window{};
		std::unique_ptr<VeRenderer> m_renderer{};
		std::unique_ptr<VeSceneManager> m_sceneManager{};
	};
};



