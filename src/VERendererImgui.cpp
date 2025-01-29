
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    RendererImgui::RendererImgui( std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallback( { 
			{this,   4000, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,   1000, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
			{this,   3000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,      0, "SDL", [this](Message& message){ return OnSDL(message);} },
			{this,   1000, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );

    };

    RendererImgui::~RendererImgui() {};

    bool RendererImgui::OnInit(Message message) {
        vh::RenCreateRenderPass(GetPhysicalDevice(), GetDevice(), GetSwapChain(), false, m_renderPass);
		
 		vh::RenCreateDescriptorSetLayout( GetDevice(), {}, m_descriptorSetLayoutPerFrame );
			
        vh::RenCreateGraphicsPipeline(GetDevice(), m_renderPass, "shaders\\Imgui\\vert.spv", "", {}, {},
			 { m_descriptorSetLayoutPerFrame }, m_graphicsPipeline);

        vh::RenCreateDescriptorPool(GetDevice(), 1000, m_descriptorPool);

		vh::VulSetupImgui( ((WindowSDL*)m_window)->GetSDLWindow(), GetInstance(), GetPhysicalDevice(), GetQueueFamilies(), GetDevice(), GetGraphicsQueue(), 
			m_commandPool, m_descriptorPool, m_renderPass);  

        vh::createCommandPool(GetSurface(), GetPhysicalDevice(), GetDevice(), m_commandPool); 
        vh::createCommandBuffers(GetDevice(), m_commandPool, m_commandBuffers);
		return false;
	}

    bool RendererImgui::OnPrepareNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return false;
	    ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
		return false;
    }

    bool RendererImgui::OnRecordNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return false;

        vkResetCommandBuffer(m_commandBuffers[GetCurrentFrame()],  0);

		vh::startRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
			GetSwapChain(), m_renderPass, m_graphicsPipeline, 
			false, ((WindowSDL*)m_window)->GetClearColor(), GetCurrentFrame());
		
		ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[GetCurrentFrame()]);

		vh::endRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()]);

		SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
		return false;
    }

    bool RendererImgui::OnSDL(Message message) {
    	SDL_Event event = message.template GetData<MsgSDL>().m_event;
    	ImGui_ImplSDL2_ProcessEvent(&event);
		return false;
    }

    bool RendererImgui::OnQuit(Message message) {
        vkDeviceWaitIdle(GetDevice());
		ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        vkDestroyCommandPool(GetDevice(), m_commandPool, nullptr);
        vkDestroyRenderPass(GetDevice(), m_renderPass, nullptr);
		vkDestroyPipeline(GetDevice(), m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(GetDevice(), m_graphicsPipeline.m_pipelineLayout, nullptr);   
        vkDestroyDescriptorPool(GetDevice(), m_descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayoutPerFrame, nullptr);
		return false;
    }


};   // namespace vve

