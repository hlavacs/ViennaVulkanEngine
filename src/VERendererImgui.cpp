
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    RendererImgui::RendererImgui( std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallbacks( { 
			{this,   4000, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,   1000, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
			{this,   3000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,      0, "SDL", [this](Message& message){ return OnSDL(message);} },
			{this,   1000, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );

    };

    RendererImgui::~RendererImgui() {};

    bool RendererImgui::OnInit(Message message) {
		Renderer::OnInit(message);

        vvh::RenCreateRenderPass({
			.m_depthFormat 	= m_vkState().m_depthMapFormat, 
			.m_device 		= m_vkState().m_device, 
			.m_swapChain 	= m_vkState().m_swapChain, 
			.m_clear 		= false, 
			.m_renderPass 	= m_renderPass
		});
		
 		vvh::RenCreateDescriptorSetLayout( {m_vkState().m_device, {}, m_descriptorSetLayoutPerFrame });
			
        vvh::RenCreateGraphicsPipeline({
			.m_device 					= m_vkState().m_device, 
			.m_renderPass 				= m_renderPass, 
			.m_vertShaderPath 			= "shaders/Imgui/vert.spv", 
			.m_fragShaderPath 			= "", 
			.m_bindingDescription		= {}, 
			.m_attributeDescriptions 	= {},
			.m_descriptorSetLayouts 	= { m_descriptorSetLayoutPerFrame }, 
			.m_specializationConstants 	= {}, 
			.m_pushConstantRanges 		= {}, 
			.m_blendAttachments 		= {}, 
			.m_graphicsPipeline 		= m_graphicsPipeline
		});

        vvh::RenCreateDescriptorPool({m_vkState().m_device, 1000, m_descriptorPool});

		vvh::SetupImgui( 
			m_windowSDLState().m_sdlWindow, 
			m_vkState().m_instance, 
			m_vkState().m_physicalDevice, 
			m_vkState().m_queueFamilies, 
			m_vkState().m_device, 
			m_vkState().m_graphicsQueue, 
			m_commandPool, 
			m_descriptorPool, 
			m_renderPass
		);  

        vvh::ComCreateCommandPool({
			.m_surface 			= m_vkState().m_surface, 
			.m_physicalDevice 	= m_vkState().m_physicalDevice, 
			.m_device 			= m_vkState().m_device, 
			.m_queueFamilyIndex	= m_vkState().m_queueFamilies.graphicsFamily.value(),
			.m_commandPool 		= m_commandPool
		}); 
        vvh::ComCreateCommandBuffers({
			.m_device 			= m_vkState().m_device, 
			.m_commandPool 		= m_commandPool, 
			.m_commandBuffers 	= m_commandBuffers
		});
		return false;
	}

    bool RendererImgui::OnPrepareNextFrame(Message message) {
	    ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
		return false;
    }

    bool RendererImgui::OnRecordNextFrame(Message message) {

        vkResetCommandBuffer(m_commandBuffers[m_vkState().m_currentFrame],  0);

		vvh::ComBeginCommandBuffer({.m_commandBuffer = m_commandBuffers[m_vkState().m_currentFrame]});

		vvh::ComBeginRenderPass({
			.m_commandBuffer 	= m_commandBuffers[m_vkState().m_currentFrame], 
			.m_imageIndex 		= m_vkState().m_imageIndex, 
			.m_swapChain 		= m_vkState().m_swapChain, 
			.m_renderPass 		= m_renderPass, 
			.m_clear 			= false, 
			.m_clearColor 		= m_windowState().m_clearColor, 
			.m_currentFrame 	= m_vkState().m_currentFrame
		});
		
		ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffers[m_vkState().m_currentFrame]);

		vvh::ComEndRenderPass({.m_commandBuffer = m_commandBuffers[m_vkState().m_currentFrame]});
		vvh::ComEndCommandBuffer({.m_commandBuffer = m_commandBuffers[m_vkState().m_currentFrame]});

		SubmitCommandBuffer(m_commandBuffers[m_vkState().m_currentFrame]);
		return false;
    }

    bool RendererImgui::OnSDL(Message message) {
    	SDL_Event event = message.template GetData<MsgSDL>().m_event;
    	ImGui_ImplSDL3_ProcessEvent(&event);
		return false;
    }

    bool RendererImgui::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vkState().m_device);
		ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        vkDestroyCommandPool(m_vkState().m_device, m_commandPool, nullptr);
        vkDestroyRenderPass(m_vkState().m_device, m_renderPass, nullptr);
		vkDestroyPipeline(m_vkState().m_device, m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_vkState().m_device, m_graphicsPipeline.m_pipelineLayout, nullptr);   
        vkDestroyDescriptorPool(m_vkState().m_device, m_descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerFrame, nullptr);
		return false;
    }


};   // namespace vve

