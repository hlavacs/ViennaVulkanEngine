
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    RendererForward::RendererForward( std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName ) {

  		engine.RegisterCallback( { 
  			{this,     0, "ANNOUNCE", [this](Message message){this->OnAnnounce(message);} },
  			{this,  3000, "INIT", [this](Message message){this->OnInit(message);} },
  			{this,  2000, "RECORD_NEXT_FRAME", [this](Message message){this->OnRecordNextFrame(message);} },
  			{this,     0, "QUIT", [this](Message message){this->OnQuit(message);} }
  		} );
    };

    RendererForward::~RendererForward(){};

    void RendererForward::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender->GetName() == "VVE Renderer Vulkan" ) {
			m_vulkan = dynamic_cast<RendererVulkan*>(msg.m_sender);
		}
    }

    void RendererForward::OnInit(Message message) {
        vh::createRenderPass(m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_vulkan->GetSwapChain(), false, m_renderPass);
        vh::createCommandPool(m_vulkan->GetSurface(), m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_commandPool);
        vh::createCommandBuffers(m_vulkan->GetDevice(), m_commandPool, m_commandBuffers);
    }

    void RendererForward::OnRecordNextFrame(Message message) {
		
		for( decltype(auto) ubo : m_registry.template GetView<vh::UniformBuffers&>() ) {
        	vh::updateUniformBuffer(m_vulkan->GetCurrentFrame(), m_vulkan->GetSwapChain(), ubo);
		}

		vkResetCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()],  0);
        
		vh::startRecordCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()], m_vulkan->GetImageIndex(), 
			m_vulkan->GetSwapChain(), m_renderPass, m_vulkan->GetGraphicsPipeline(), 
			false, ((WindowSDL*)m_window)->GetClearColor(), m_vulkan->GetCurrentFrame());
		
		for( auto[ghandle, ubo, ds] : m_registry.template GetView<GeometryHandle, vh::UniformBuffers&, vh::DescriptorSets&>() ) {
			auto& geometry = m_registry.template Get<vh::Geometry&>(ghandle);
			vh::recordObject2( m_commandBuffers[m_vulkan->GetCurrentFrame()], m_vulkan->GetGraphicsPipeline(), ds, geometry, m_vulkan->GetCurrentFrame() );
		}

		vh::endRecordCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()]);

	    m_vulkan->SubmitCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()]);
    }

    void RendererForward::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vulkan->GetDevice());
        vkDestroyRenderPass(m_vulkan->GetDevice(), m_renderPass, nullptr);
        vkDestroyCommandPool(m_vulkan->GetDevice(), m_commandPool, nullptr);
    }


};   // namespace vve


