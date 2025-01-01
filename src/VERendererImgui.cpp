
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    RendererImgui::RendererImgui( std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallback( { 
  			{this,     0, "ANNOUNCE", [this](Message message){OnAnnounce(message);} },
			{this,   4000, "INIT", [this](Message message){OnInit(message);} },
			{this,   1000, "PREPARE_NEXT_FRAME", [this](Message message){OnPrepareNextFrame(message);} },
			{this,   3000, "RECORD_NEXT_FRAME", [this](Message message){OnRecordNextFrame(message);} },
			{this,      0, "SDL", [this](Message message){OnSDL(message);} },
			{this,   1000, "QUIT", [this](Message message){OnQuit(message);} }
		} );

    };

    RendererImgui::~RendererImgui() {};

    void RendererImgui::OnAnnounce(Message message) {
		auto msg = message.template GetData<MsgAnnounce>();
		if( msg.m_sender->GetName() == "VVE Renderer Vulkan" ) {
			m_vulkan = dynamic_cast<RendererVulkan*>(msg.m_sender);
		}
    }

    void RendererImgui::OnInit(Message message) {
        vh::createRenderPass(m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_vulkan->GetSwapChain(), false, m_renderPass);
		
		vh::createDescriptorSetLayout(m_vulkan->GetDevice(),
			{
				{
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
				},
				{
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT			
				}
			},
			m_descriptorSetLayouts
		);

		vh::createGraphicsPipeline(m_vulkan->GetDevice(), m_renderPass, m_descriptorSetLayouts, m_graphicsPipeline);

		vh::setupImgui( ((WindowSDL*)m_window)->GetSDLWindow(), m_vulkan->GetInstance(), m_vulkan->GetPhysicalDevice(), m_vulkan->GetQueueFamilies(), m_vulkan->GetDevice(), m_vulkan->GetGraphicsQueue(), 
			m_vulkan->GetCommandPool(), m_vulkan->GetDescriptorPool(), m_renderPass);  

        vh::createCommandPool(m_vulkan->GetSurface(), m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_commandPool); 
        vh::createCommandBuffers(m_vulkan->GetDevice(), m_commandPool, m_commandBuffers);
	}

    void RendererImgui::OnPrepareNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return;
	    ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void RendererImgui::OnRecordNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return;

        vkResetCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()],  0);

		vh::startRecordCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()], m_vulkan->GetImageIndex(), 
			m_vulkan->GetSwapChain(), m_renderPass, m_graphicsPipeline, 
			false, ((WindowSDL*)m_window)->GetClearColor(), m_vulkan->GetCurrentFrame());
		
		ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[m_vulkan->GetCurrentFrame()]);

		vh::endRecordCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()]);

		m_vulkan->SubmitCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()]);
    }

    void RendererImgui::OnSDL(Message message) {
    	SDL_Event event = message.template GetData<MsgSDL>().m_event;
    	ImGui_ImplSDL2_ProcessEvent(&event);
    }

    void RendererImgui::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vulkan->GetDevice());
		ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        vkDestroyCommandPool(m_vulkan->GetDevice(), m_commandPool, nullptr);
        vkDestroyRenderPass(m_vulkan->GetDevice(), m_renderPass, nullptr);
		vkDestroyPipeline(m_vulkan->GetDevice(), m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_vulkan->GetDevice(), m_graphicsPipeline.m_pipelineLayout, nullptr);   
		for( auto layout : m_descriptorSetLayouts.m_descriptorSetLayouts ) {
			vkDestroyDescriptorSetLayout(m_vulkan->GetDevice(), layout, nullptr);
		}
    }


};   // namespace vve

