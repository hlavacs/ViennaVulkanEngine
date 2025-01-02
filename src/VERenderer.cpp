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
	};

	auto Renderer::GetSurface() -> VkSurfaceKHR { return m_vulkan->GetSurface(); };
	auto Renderer::GetInstance() -> VkInstance { return m_vulkan->GetInstance(); }
	auto Renderer::GetPhysicalDevice() -> VkPhysicalDevice { return m_vulkan->GetPhysicalDevice(); }
	auto Renderer::GetDevice() -> VkDevice { return m_vulkan->GetDevice(); }
	auto Renderer::GetQueueFamilies() -> vh::QueueFamilyIndices { return m_vulkan->GetQueueFamilies(); }
	auto Renderer::GetGraphicsQueue() -> VkQueue { return m_vulkan->GetGraphicsQueue(); }
	auto Renderer::GetPresentQueue() -> VkQueue { return m_vulkan->GetPresentQueue(); }
	auto Renderer::GetCommandPool() -> VkCommandPool { return m_vulkan->GetCommandPool(); }
	auto Renderer::GetDescriptorPool() -> VkDescriptorPool { return m_vulkan->GetDescriptorPool(); }
	auto Renderer::GetVmaAllocator() -> VmaAllocator& { return m_vulkan->GetVmaAllocator(); }
	auto Renderer::GetSwapChain() -> vh::SwapChain& { return m_vulkan->GetSwapChain(); }
	auto Renderer::GetDepthImage() -> vh::DepthImage& { return m_vulkan->GetDepthImage(); }
	auto Renderer::GetCurrentFrame() -> uint32_t { return m_vulkan->GetCurrentFrame(); }
	auto Renderer::GetImageIndex() -> uint32_t { return m_vulkan->GetImageIndex(); }
	void Renderer::SubmitCommandBuffer( VkCommandBuffer commandBuffer ) { m_vulkan->SubmitCommandBuffer(commandBuffer); };

};  // namespace vve

