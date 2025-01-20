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

	auto Renderer::GetSurface() -> VkSurfaceKHR& { return GetVulkanState().m_surface; };
	auto Renderer::GetInstance() -> VkInstance& { return GetVulkanState().m_instance; }
	auto Renderer::GetPhysicalDevice() -> VkPhysicalDevice& { return GetVulkanState().m_physicalDevice; }
	auto Renderer::GetDevice() -> VkDevice& { return GetVulkanState().m_device; }
	auto Renderer::GetQueueFamilies() -> vh::QueueFamilyIndices& { return GetVulkanState().m_queueFamilies; }
	auto Renderer::GetGraphicsQueue() -> VkQueue& { return GetVulkanState().m_graphicsQueue; }
	auto Renderer::GetPresentQueue() -> VkQueue& { return GetVulkanState().m_presentQueue; }
	auto Renderer::GetVmaAllocator() -> VmaAllocator& { return GetVulkanState().m_vmaAllocator; }
	auto Renderer::GetSwapChain() -> vh::SwapChain& { return GetVulkanState().m_swapChain; }
	auto Renderer::GetCurrentFrame() -> uint32_t& { return GetVulkanState().m_currentFrame; }
	auto Renderer::GetImageIndex() -> uint32_t& { return GetVulkanState().m_imageIndex; }
	auto Renderer::GetFramebufferResized() -> bool& { return GetVulkanState().m_framebufferResized; }
	auto Renderer::GetDepthImage() -> vh::DepthImage& { return GetVulkanState().m_depthImage; }

	void Renderer::SubmitCommandBuffer( VkCommandBuffer commandBuffer ) { 
		GetVulkanState().m_commandBuffersSubmit.push_back(commandBuffer); /////////////////////
		//std::cout << "SubmitCommandBuffer: " << m_commandBuffersSubmit.size() << std::endl;
	};

	auto Renderer::GetVulkanState() -> VulkanState& { 
		return m_vulkan->GetVulkanState();
	}

	/*auto Renderer::GetVulkanState() -> VulkanState& { 
		if( m_vulkanStateHandle().IsValid() == false ) {
			for( auto [handle, state] : m_registry.template GetView<vecs::Handle, VulkanState&>() ) {
				m_vulkanStateHandle = handle;
				return state;
			}
			m_vulkanStateHandle = m_registry.template Insert(VulkanState{});
		}
		auto& state = m_registry.template Get<VulkanState&>(m_vulkanStateHandle);
		return state;
	}*/


	//void Renderer::SubmitCommandBuffer( VkCommandBuffer commandBuffer ) { assert(m_vulkan!=this); m_vulkan->SubmitCommandBuffer(commandBuffer); };

};  // namespace vve

