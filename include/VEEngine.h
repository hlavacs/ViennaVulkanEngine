#pragma once

#include <memory>
#include <unordered_map>
#include "glm/glm.hpp"
#include <vulkan/vulkan.h>
#include "VEInclude.h"
#include "VHDevice.h"
#include "VEWindow.h"
#include "VERendererForward.h"
#include "VESceneManager.h"
#include "VESystem.h"
#include "VECS.h"


namespace vve {

   	template<ArchitectureType ATYPE>
	class SceneManager;

	template<ArchitectureType ATYPE = ArchitectureType::SEQUENTIAL>
	class Engine  {

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
		Engine();
		virtual ~Engine();
		void RegisterSystem( std::shared_ptr<System<ATYPE>> system, std::vector<MessageType> messageTypes );
		void DeregisterSystem( std::shared_ptr<System<ATYPE>> system );
		void Run();
		void Stop();
		auto getState() -> const VulkanState& { return m_state; }
		auto getWindow() -> std::shared_ptr<Window<ATYPE>> { return m_window; }
		auto getRenderer() -> std::shared_ptr<Renderer<ATYPE>> { return m_renderer; }
		auto getSceneMgr() -> std::shared_ptr<SceneManager<ATYPE>> { return m_sceneManager; }
		auto getRegistry() -> vecs::Registry<>& { return m_registry; }
		void SendMessage( Message message );

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
		
		using MessageMap = std::unordered_map<MessageType, std::set<std::shared_ptr<System<ATYPE>>>>;
		MessageMap m_messageMap{};

		std::shared_ptr<Window<ATYPE>> m_window{};
		std::shared_ptr<Renderer<ATYPE>> m_renderer;
		std::shared_ptr<SceneManager<ATYPE>> m_sceneManager;
	};
};



