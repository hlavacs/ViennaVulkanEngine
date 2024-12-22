
#include "VHInclude.h"
#include "VHVulkan.h"
#include "VESystem.h"
#include "VEEngine.h"
#include "VEWindowSDL.h"
#include "VERendererVulkan.h"
#include "VERendererForward.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::RendererForward( std::string systemName, Engine<ATYPE>& engine, Window<ATYPE>* window ) 
        : Renderer<ATYPE>(systemName, engine, window ) {

  		engine.RegisterCallback( { 
  			{this, -1000, "INIT", [this](Message message){this->OnInit(message);} },
  			{this,  2000, "INIT", [this](Message message){this->OnInit2(message);} },
  			{this,  2000, "RECORD_NEXT_FRAME", [this](Message message){this->OnRecordNextFrame(message);} },
  			{this, -3000, "QUIT", [this](Message message){this->OnQuit(message);} }
  		} );
    };

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::~RendererForward(){};

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnInit(Message message) {
    }

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnInit2(Message message) {
		if(m_vulkan == nullptr) { m_vulkan = (RendererVulkan<ATYPE>*)m_engine.GetSystem("VVE RendererVulkan"); }
        vh::createRenderPass(m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_vulkan->GetSwapChain(), false, m_renderPass);
        vh::createCommandPool(m_window->GetSurface(), m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_commandPool);
        vh::createCommandBuffers(m_vulkan->GetDevice(), m_commandPool, m_commandBuffers);
    }

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnRecordNextFrame(Message message) {
        vh::updateUniformBuffer(m_vulkan->GetCurrentFrame(), m_vulkan->GetSwapChain(), m_vulkan->GetUniformBuffers());
        
		vkResetCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()],  0);
        
		vh::startRecordCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()], m_vulkan->GetImageIndex(), 
			m_vulkan->GetSwapChain(), m_renderPass, m_vulkan->GetGraphicsPipeline(), m_vulkan->GetDescriptorSets(), 
			false, ((WindowSDL<ATYPE>*)m_window)->GetClearColor(), m_vulkan->GetCurrentFrame());
		
		for( decltype(auto) geometry : m_registry.template GetView<vh::Geometry&>() ) {
			vh::recordObject( m_commandBuffers[m_vulkan->GetCurrentFrame()], m_vulkan->GetGraphicsPipeline(), m_vulkan->GetDescriptorSets(), geometry, m_vulkan->GetCurrentFrame() );
		}

		vh::endRecordCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()]);

	    m_vulkan->SubmitCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()]);
    }


   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vulkan->GetDevice());
        vkDestroyRenderPass(m_vulkan->GetDevice(), m_renderPass, nullptr);
        vkDestroyCommandPool(m_vulkan->GetDevice(), m_commandPool, nullptr);
    }

    template class RendererForward<ENGINETYPE_SEQUENTIAL>;
    template class RendererForward<ENGINETYPE_PARALLEL>;

};   // namespace vve


