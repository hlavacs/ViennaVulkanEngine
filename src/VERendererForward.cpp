
#include "VHInclude.h"
#include "VHVulkan.h"
#include "VESystem.h"
#include "VEEngine.h"
#include "VEWindowSDL.h"
#include "VERendererVulkan.h"
#include "VERendererForward.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::RendererForward( std::string systemName, Engine<ATYPE>* engine, Window<ATYPE>* window ) 
        : Renderer<ATYPE>(systemName, engine, window ) {

  		engine->RegisterCallback( { 
  			{this, -5000, MsgType::INIT, [this](Message message){this->OnInit(message);} },
  			{this,  5000, MsgType::RECORD_NEXT_FRAME, [this](Message message){this->OnRecordNextFrame(message);} },
  			{this, -5000, MsgType::QUIT, [this](Message message){this->OnQuit(message);} }
  		} );
    };

   	template<ArchitectureType ATYPE>
    RendererForward<ATYPE>::~RendererForward(){};


   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnInit(Message message) {

    }

   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnRecordNextFrame(Message message) {
		if(m_vulkan == nullptr) {
			m_vulkan = (RendererVulkan<ATYPE>*)m_engine->GetSystem("VVE RendererVulkan");
		}

        vh::updateUniformBuffer(m_vulkan->GetCurrentFrame(), m_vulkan->GetSwapChain(), m_vulkan->GetUniformBuffers());
        vkResetCommandBuffer(m_vulkan->GetCommandBuffers()[m_vulkan->GetCurrentFrame()],  0);
        
		recordCommandBuffer(
			m_vulkan->GetCommandBuffers()[m_vulkan->GetCurrentFrame()], m_vulkan->GetImageIndex(), 
			m_vulkan->GetSwapChain(), m_vulkan->GetRenderPass(), m_vulkan->GetGraphicsPipeline(), 
			m_vulkan->GetGeometry(), m_vulkan->GetDescriptorSets(), ((WindowSDL<ATYPE>*)m_window)->GetClearColor(), 
			m_vulkan->GetCurrentFrame());
	        
    }


   	template<ArchitectureType ATYPE>
    void RendererForward<ATYPE>::OnQuit(Message message) {

    }

    template class RendererForward<ENGINETYPE_SEQUENTIAL>;
    template class RendererForward<ENGINETYPE_PARALLEL>;

};   // namespace vve


