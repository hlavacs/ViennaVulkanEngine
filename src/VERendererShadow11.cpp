#include "VHInclude.h"
#include "VEInclude.h"



namespace vve {

	RendererShadow11::RendererShadow11( std::string systemName, Engine& engine, std::string windowName ) : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallback( { 
			{this,  3500, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,  1500, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
			{this,  1500, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} }, 
			{this,     0, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );
	};

	RendererShadow11::~RendererShadow11(){};

	bool RendererShadow11::OnInit(Message message) {
		Renderer::OnInit(message);

		vh::RenCreateRenderPass(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_swapChain, false, m_renderPass);

		vh::RenCreateDescriptorSetLayout( m_vulkanState().m_device, //Per frame
			{ 
				{ 	.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
				{ 	.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			m_descriptorSetLayoutPerFrame );

		vh::ComCreateCommandPool(m_vulkanState().m_surface, m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_commandPool);
		vh::ComCreateCommandBuffers(m_vulkanState().m_device, m_commandPool, m_commandBuffers);
		vh::RenCreateDescriptorPool(m_vulkanState().m_device, 1000, m_descriptorPool);

		vh::RenCreateDescriptorSet(m_vulkanState().m_device, m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetPerFrame);
		vh::BufCreateUniformBuffers(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, sizeof(vh::UniformBufferFrame), m_uniformBuffersPerFrame);
		vh::RenUpdateDescriptorSetUBO(m_vulkanState().m_device, m_uniformBuffersPerFrame, 0, sizeof(vh::UniformBufferFrame), m_descriptorSetPerFrame);   

		vh::BufCreateUniformBuffers(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_maxNumberLights*sizeof(vh::Light), m_uniformBuffersLights);
		vh::RenUpdateDescriptorSetUBO(m_vulkanState().m_device, m_uniformBuffersLights, 1, m_maxNumberLights*sizeof(vh::Light), m_descriptorSetPerFrame);   

		return false;
	}

	bool RendererShadow11::OnPrepareNextFrame(Message message) {
		auto msg = message.template GetData<MsgPrepareNextFrame>();
		return false;
	}

	bool RendererShadow11::OnRecordNextFrame(Message message) {
		auto msg = message.template GetData<MsgRecordNextFrame>();
		return false;
	}

	bool RendererShadow11::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vulkanState().m_device);
		
        vkDestroyCommandPool(m_vulkanState().m_device, m_commandPool, nullptr);

		//vkDestroyDescriptorSetLayout(m_vulkanState().m_device, m_descriptorSetLayoutPerObject, nullptr);
		//vkDestroyPipeline(m_vulkanState().m_device, m_graphicsPipeline.m_pipeline, nullptr);
		//vkDestroyPipelineLayout(m_vulkanState().m_device, m_graphicsPipeline.m_pipelineLayout, nullptr);
	
        vkDestroyDescriptorPool(m_vulkanState().m_device, m_descriptorPool, nullptr);
		vkDestroyRenderPass(m_vulkanState().m_device, m_renderPass, nullptr);
		vh::BufDestroyBuffer2(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_uniformBuffersPerFrame);
		vh::BufDestroyBuffer2(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_uniformBuffersLights);
		vkDestroyDescriptorSetLayout(m_vulkanState().m_device, m_descriptorSetLayoutPerFrame, nullptr);

		return false;
    }


};   // namespace vve