#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {
    
    Renderer::Renderer(std::string systemName, Engine& engine, std::string windowName ) : 
		System{systemName, engine }, m_vulkan{nullptr},  m_windowName(windowName) {
		engine.RegisterCallback( { 
			{this,      0, "ANNOUNCE", [this](Message& message){ return OnAnnounce(message);} }
		} );
	};

    Renderer::~Renderer(){};

    bool Renderer::OnAnnounce( Message message ) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender->GetName() == m_windowName ) {
			m_window = dynamic_cast<Window*>(msg.m_sender);
		}
		if( msg.m_sender->GetName() == m_engine.m_rendererVulkanaName && m_vulkan == nullptr ) {
			m_vulkan = dynamic_cast<RendererVulkan*>(msg.m_sender);
		}
		return false;
	};

	auto Renderer::GetSurface() -> VkSurfaceKHR { return GetVulkanState().m_surface; };
	auto Renderer::GetInstance() -> VkInstance { return GetVulkanState().m_instance; }
	auto Renderer::GetPhysicalDevice() -> VkPhysicalDevice { return GetVulkanState().m_physicalDevice; }
	auto Renderer::GetDevice() -> VkDevice { return GetVulkanState().m_device; }
	auto Renderer::GetQueueFamilies() -> vh::QueueFamilyIndices { return GetVulkanState().m_queueFamilies; }
	auto Renderer::GetGraphicsQueue() -> VkQueue { return GetVulkanState().m_graphicsQueue; }
	auto Renderer::GetPresentQueue() -> VkQueue { return GetVulkanState().m_presentQueue; }
	auto Renderer::GetVmaAllocator() -> VmaAllocator& { return GetVulkanState().m_vmaAllocator; }
	auto Renderer::GetSwapChain() -> vh::SwapChain& { return GetVulkanState().m_swapChain; }
	auto Renderer::GetCurrentFrame() -> uint32_t& { return GetVulkanState().m_currentFrame; }
	auto Renderer::GetImageIndex() -> uint32_t& { return GetVulkanState().m_imageIndex; }

	auto Renderer::GetDepthImage() -> vh::DepthImage& { assert(m_vulkan!=this); return m_vulkan->GetDepthImage(); }
	auto Renderer::GetCommandPool() -> VkCommandPool { assert(m_vulkan!=this); return m_vulkan->GetCommandPool(); }
	auto Renderer::GetDescriptorPool() -> VkDescriptorPool { assert(m_vulkan!=this); return m_vulkan->GetDescriptorPool(); }
	/*
	auto Renderer::GetSurface() -> VkSurfaceKHR { assert(m_vulkan!=this); return m_vulkan->GetSurface(); };
	auto Renderer::GetInstance() -> VkInstance { assert(m_vulkan!=this); return m_vulkan->GetInstance(); }
	auto Renderer::GetPhysicalDevice() -> VkPhysicalDevice { assert(m_vulkan!=this); return m_vulkan->GetPhysicalDevice(); }
	auto Renderer::GetDevice() -> VkDevice { assert(m_vulkan!=this); return m_vulkan->GetDevice(); }
	auto Renderer::GetQueueFamilies() -> vh::QueueFamilyIndices { assert(m_vulkan!=this); return m_vulkan->GetQueueFamilies(); }
	auto Renderer::GetGraphicsQueue() -> VkQueue { assert(m_vulkan!=this); return m_vulkan->GetGraphicsQueue(); }
	auto Renderer::GetPresentQueue() -> VkQueue { assert(m_vulkan!=this); return m_vulkan->GetPresentQueue(); }
	auto Renderer::GetCommandPool() -> VkCommandPool { assert(m_vulkan!=this); return m_vulkan->GetCommandPool(); }
	auto Renderer::GetDescriptorPool() -> VkDescriptorPool { assert(m_vulkan!=this); return m_vulkan->GetDescriptorPool(); }
	auto Renderer::GetVmaAllocator() -> VmaAllocator& { assert(m_vulkan!=this); return m_vulkan->GetVmaAllocator(); }
	auto Renderer::GetSwapChain() -> vh::SwapChain& { assert(m_vulkan!=this); return m_vulkan->GetSwapChain(); }
	auto Renderer::GetDepthImage() -> vh::DepthImage& { assert(m_vulkan!=this); return m_vulkan->GetDepthImage(); }
	auto Renderer::GetCurrentFrame() -> uint32_t& { assert(m_vulkan!=this); return m_vulkan->GetCurrentFrame(); }
	auto Renderer::GetImageIndex() -> uint32_t& { assert(m_vulkan!=this); return m_vulkan->GetImageIndex(); }
	*/


	auto Renderer::GetVulkanState() -> VulkanState& { 
		return m_vulkan->GetVulkanState();
	}


	/*auto Renderer::GetVulkanState() -> VulkanState& { 
		if( m_vulkanStateHandle().IsValid() == false ) {
			auto [handle, state] = *m_registry.template GetView<vecs::Handle, VulkanState&>().begin();
			m_vulkanStateHandle = handle;
			return state;
		}
		return m_registry.template Get<VulkanState&>(m_vulkanStateHandle);
	}*/


	void Renderer::SubmitCommandBuffer( VkCommandBuffer commandBuffer ) { assert(m_vulkan!=this); m_vulkan->SubmitCommandBuffer(commandBuffer); };

};  // namespace vve

