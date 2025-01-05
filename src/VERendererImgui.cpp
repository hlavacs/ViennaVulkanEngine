
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    RendererImgui::RendererImgui( std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallback( { 
			{this,   4000, "INIT", [this](Message message){OnInit(message);} },
			{this,   1000, "PREPARE_NEXT_FRAME", [this](Message message){OnPrepareNextFrame(message);} },
			{this,   3000, "RECORD_NEXT_FRAME", [this](Message message){OnRecordNextFrame(message);} },
			{this,      0, "SDL", [this](Message message){OnSDL(message);} },
			{this,   1000, "QUIT", [this](Message message){OnQuit(message);} }
		} );

    };

    RendererImgui::~RendererImgui() {};

    void RendererImgui::OnInit(Message message) {
        vh::createRenderPass(GetPhysicalDevice(), GetDevice(), GetSwapChain(), false, m_renderPass);
		
		vh::createDescriptorSetLayout(GetDevice(),
			{ { .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT },
			{ .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 	.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT } },
			m_descriptorSetLayouts
		);

		vh::createGraphicsPipeline(GetDevice(), m_renderPass, "shaders\\vert.spv", "shaders\\frag.spv",
			 { m_descriptorSetLayouts }, m_graphicsPipeline);

		vh::setupImgui( ((WindowSDL*)m_window)->GetSDLWindow(), GetInstance(), GetPhysicalDevice(), GetQueueFamilies(), GetDevice(), GetGraphicsQueue(), 
			GetCommandPool(), GetDescriptorPool(), m_renderPass);  

        vh::createCommandPool(GetSurface(), GetPhysicalDevice(), GetDevice(), m_commandPool); 
        vh::createCommandBuffers(GetDevice(), m_commandPool, m_commandBuffers);
	}

    void RendererImgui::OnPrepareNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return;
	    ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void RendererImgui::OnRecordNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return;

        vkResetCommandBuffer(m_commandBuffers[GetCurrentFrame()],  0);

		vh::startRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
			GetSwapChain(), m_renderPass, m_graphicsPipeline, 
			false, ((WindowSDL*)m_window)->GetClearColor(), GetCurrentFrame());
		
		ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[GetCurrentFrame()]);

		vh::endRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()]);

		SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
    }

    void RendererImgui::OnSDL(Message message) {
    	SDL_Event event = message.template GetData<MsgSDL>().m_event;
    	ImGui_ImplSDL2_ProcessEvent(&event);
    }

    void RendererImgui::OnQuit(Message message) {
        vkDeviceWaitIdle(GetDevice());
		ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        vkDestroyCommandPool(GetDevice(), m_commandPool, nullptr);
        vkDestroyRenderPass(GetDevice(), m_renderPass, nullptr);
		vkDestroyPipeline(GetDevice(), m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(GetDevice(), m_graphicsPipeline.m_pipelineLayout, nullptr);   
		vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayouts, nullptr);
    }


};   // namespace vve

