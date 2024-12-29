
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::RendererImgui( std::string systemName, Engine<ATYPE>& engine ) 
        : Renderer<ATYPE>(systemName, engine ) {

		engine.RegisterCallback( { 
  			{this,     0, "ANNOUNCE", [this](Message message){this->OnAnnounce(message);} },
			{this,   3000, "INIT", [this](Message message){this->OnInit(message);} },
			{this,   1000, "PREPARE_NEXT_FRAME", [this](Message message){this->OnPrepareNextFrame(message);} },
			{this,   3000, "RECORD_NEXT_FRAME", [this](Message message){this->OnRecordNextFrame(message);} },
			{this,      0, "SDL", [this](Message message){this->OnSDL(message);} },
			{this,  -2000, "QUIT", [this](Message message){this->OnQuit(message);} }
		} );

    };

   	template<ArchitectureType ATYPE>
    RendererImgui<ATYPE>::~RendererImgui() {};

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender->GetName() == "VVE Renderer Vulkan" ) {
			m_vulkan = (RendererVulkan<ATYPE>*)msg.m_sender;
		}
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnInit(Message message) {
		//if(m_vulkan == nullptr) { m_vulkan = (RendererVulkan<ATYPE>*)m_engine.GetSystem("VVE Renderer Vulkan"); }

		WindowSDL<ATYPE>* windowSDL = (WindowSDL<ATYPE>*)m_window;

        vh::createRenderPass(m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_vulkan->GetSwapChain(), false, m_renderPass);
		
		vh::setupImgui(windowSDL->GetSDLWindow(), m_vulkan->GetInstance(), m_vulkan->GetPhysicalDevice(), m_vulkan->GetQueueFamilies(), m_vulkan->GetDevice(), m_vulkan->GetGraphicsQueue(), 
			m_vulkan->GetCommandPool(), m_vulkan->GetDescriptorPool(), m_renderPass);  

        vh::createCommandPool(m_window->GetSurface(), m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_commandPool); 
        vh::createCommandBuffers(m_vulkan->GetDevice(), m_commandPool, m_commandBuffers);
	}

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnPrepareNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return;
	    ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnRecordNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return;

        vkResetCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()],  0);

		vh::startRecordCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()], m_vulkan->GetImageIndex(), 
			m_vulkan->GetSwapChain(), m_renderPass, m_vulkan->GetGraphicsPipeline(), 
			false, ((WindowSDL<ATYPE>*)m_window)->GetClearColor(), m_vulkan->GetCurrentFrame());
		
		ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[m_vulkan->GetCurrentFrame()]);

		vh::endRecordCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()]);

		m_vulkan->SubmitCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()]);
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnSDL(Message message) {
    	SDL_Event event = message.template GetData<MsgSDL>().m_event;
    	ImGui_ImplSDL2_ProcessEvent(&event);
    }

   	template<ArchitectureType ATYPE>
    void RendererImgui<ATYPE>::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vulkan->GetDevice());
		ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        vkDestroyRenderPass(m_vulkan->GetDevice(), m_renderPass, nullptr);
        vkDestroyCommandPool(m_vulkan->GetDevice(), m_commandPool, nullptr);
    }

    template class RendererImgui<ENGINETYPE_SEQUENTIAL>;
    template class RendererImgui<ENGINETYPE_PARALLEL>;

};   // namespace vve

