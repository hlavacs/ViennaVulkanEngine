#pragma once

#include <memory>
#include <unordered_map>
#include "glm/glm.hpp"
#include <vulkan/vulkan.h>
#include "imgui.h"
#include "VECS.h"


namespace vve {

	template<ArchitectureType ATYPE = ArchitectureType::SEQUENTIAL>
	class Engine : public System<ATYPE> {

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
		void RegisterSystem( System<ATYPE>* system, int phase, std::vector<MessageType> messageTypes );
		void DeregisterSystem( System<ATYPE>* system );
		void Run();
		void Stop();
		auto GetState() -> const VulkanState& { return m_state; }
		auto GetWindows() -> std::vector<std::shared_ptr<Window<ATYPE>>>& { return m_windows; }
		auto GetSceneMgr() -> std::shared_ptr<SceneManager<ATYPE>> { return m_sceneManager; }
		auto GetRegistry() -> vecs::Registry<>& { return m_registry; }
		void SendMessage( Message message );

	protected:
		virtual void OnInit(Message message) override;
		virtual void OnQuit(Message message) override;
		void SetupVulkan();

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
		bool m_running{false};

		vecs::Registry<> m_registry;
		
		using PriorityMap = std::multimap<int, System<ATYPE>*>;
		using MessageMap = std::unordered_map<MessageType, PriorityMap>;
		MessageMap m_messageMap{};

		std::vector<std::shared_ptr<Window<ATYPE>>> m_windows{};
		std::shared_ptr<SceneManager<ATYPE>> m_sceneManager;
	};

};  // namespace vve



