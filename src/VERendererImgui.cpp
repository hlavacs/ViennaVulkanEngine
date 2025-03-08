
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
		auto vstate = GetState2();

        vh::RenCreateRenderPass(vstate().m_physicalDevice, vstate().m_device, vstate().m_swapChain, false, m_renderPass);
		
 		vh::RenCreateDescriptorSetLayout( vstate().m_device, {}, m_descriptorSetLayoutPerFrame );
			
        vh::RenCreateGraphicsPipeline(vstate().m_device, m_renderPass, "shaders\\Imgui\\vert.spv", "", {}, {},
			 { m_descriptorSetLayoutPerFrame }, {}, m_graphicsPipeline);

        vh::RenCreateDescriptorPool(vstate().m_device, 1000, m_descriptorPool);

		auto wsdlstate = WindowSDL::GetState(m_registry);

		vh::VulSetupImgui( std::get<2>(wsdlstate)().m_sdlWindow, 
			vstate().m_instance, vstate().m_physicalDevice, 
			vstate().m_queueFamilies, vstate().m_device, vstate().m_graphicsQueue, 
			m_commandPool, m_descriptorPool, m_renderPass);  

        vh::ComCreateCommandPool(vstate().m_surface, vstate().m_physicalDevice, vstate().m_device, m_commandPool); 
        vh::ComCreateCommandBuffers(vstate().m_device, m_commandPool, m_commandBuffers);
		return false;
	}

    bool RendererImgui::OnPrepareNextFrame(Message message) {
		auto [handle, wstate] = Window::GetState(m_registry, m_windowName);

        if(wstate().m_isMinimized) return false;
	    ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
		return false;
    }

    bool RendererImgui::OnRecordNextFrame(Message message) {
		auto [handle, wstate] = Window::GetState(m_registry, m_windowName);
		auto vstate = GetState2();

        if(wstate().m_isMinimized) return false;

        vkResetCommandBuffer(m_commandBuffers[vstate().m_currentFrame],  0);

		vh::ComStartRecordCommandBuffer(m_commandBuffers[vstate().m_currentFrame], vstate().m_imageIndex, 
			vstate().m_swapChain, m_renderPass, m_graphicsPipeline, 
			false, 
			std::get<1>(Window::GetState(m_registry, m_windowName))().m_clearColor, 
			vstate().m_currentFrame);
		
		ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[vstate().m_currentFrame]);

		vh::ComEndRecordCommandBuffer(m_commandBuffers[vstate().m_currentFrame]);

		SubmitCommandBuffer(m_commandBuffers[vstate().m_currentFrame]);
		return false;
    }

    bool RendererImgui::OnSDL(Message message) {
    	SDL_Event event = message.template GetData<MsgSDL>().m_event;
    	ImGui_ImplSDL2_ProcessEvent(&event);
		return false;
    }

    bool RendererImgui::OnQuit(Message message) {
		auto vstate = GetState2();

        vkDeviceWaitIdle(vstate().m_device);
		ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();

        vkDestroyCommandPool(vstate().m_device, m_commandPool, nullptr);
        vkDestroyRenderPass(vstate().m_device, m_renderPass, nullptr);
		vkDestroyPipeline(vstate().m_device, m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(vstate().m_device, m_graphicsPipeline.m_pipelineLayout, nullptr);   
        vkDestroyDescriptorPool(vstate().m_device, m_descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(vstate().m_device, m_descriptorSetLayoutPerFrame, nullptr);
		return false;
    }


};   // namespace vve

