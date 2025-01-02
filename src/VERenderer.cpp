#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {
    
    Renderer::Renderer(std::string systemName, Engine& engine, std::string windowName ) : 
		System{systemName, engine }, m_vulkan{nullptr},  m_windowName(windowName) {
		engine.RegisterCallback( { 
			{this,      0, "ANNOUNCE", [this](Message message){OnAnnounce(message);} }
		} );
	};

    Renderer::~Renderer(){};

    void Renderer::OnAnnounce( Message message ) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender->GetName() == m_windowName ) {
			m_window = dynamic_cast<Window*>(msg.m_sender);
		}
		if( msg.m_sender->GetName() == "VVE Renderer Vulkan" && m_vulkan == nullptr ) {
			m_vulkan = dynamic_cast<RendererVulkan*>(msg.m_sender);
		}
	};

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
	auto Renderer::GetCurrentFrame() -> uint32_t { assert(m_vulkan!=this); return m_vulkan->GetCurrentFrame(); }
	auto Renderer::GetImageIndex() -> uint32_t { assert(m_vulkan!=this); return m_vulkan->GetImageIndex(); }
	void Renderer::SubmitCommandBuffer( VkCommandBuffer commandBuffer ) { assert(m_vulkan!=this); m_vulkan->SubmitCommandBuffer(commandBuffer); };

};  // namespace vve

