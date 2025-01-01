
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    RendererForward::RendererForward( std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName ) {

  		engine.RegisterCallback( { 
  			{this,     0, "ANNOUNCE", [this](Message message){OnAnnounce(message);} },
  			{this,  3000, "INIT", [this](Message message){OnInit(message);} },
  			{this,  2000, "RECORD_NEXT_FRAME", [this](Message message){OnRecordNextFrame(message);} },
			{this,     0, "OBJECT_CREATE", [this](Message message){OnObjectCreate(message);} },
  			{this,     0, "QUIT", [this](Message message){OnQuit(message);} }
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
		vh::createDescriptorSetLayout(m_vulkan->GetDevice(), m_descriptorSetLayouts);
		vh::createGraphicsPipeline(m_vulkan->GetDevice(), m_renderPass, m_descriptorSetLayouts, m_graphicsPipeline);

        vh::createCommandPool(m_vulkan->GetSurface(), m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_commandPool);
        vh::createCommandBuffers(m_vulkan->GetDevice(), m_commandPool, m_commandBuffers);
    }

    void RendererForward::OnRecordNextFrame(Message message) {
		
		for( decltype(auto) ubo : m_registry.template GetView<vh::UniformBuffers&>() ) {
        	vh::updateUniformBuffer(m_vulkan->GetCurrentFrame(), m_vulkan->GetSwapChain(), ubo);
		}

		vkResetCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()],  0);
        
		vh::startRecordCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()], m_vulkan->GetImageIndex(), 
			m_vulkan->GetSwapChain(), m_renderPass, m_graphicsPipeline, 
			false, ((WindowSDL*)m_window)->GetClearColor(), m_vulkan->GetCurrentFrame());
		
		for( auto[ghandle, ubo, descriptorsets] : m_registry.template GetView<GeometryHandle, vh::UniformBuffers&, vh::DescriptorSet&>() ) {
			auto& geometry = m_registry.template Get<vh::Geometry&>(ghandle);
			vh::recordObject2( m_commandBuffers[m_vulkan->GetCurrentFrame()], m_graphicsPipeline, descriptorsets, geometry, m_vulkan->GetCurrentFrame() );
		}

		vh::endRecordCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()]);

	    m_vulkan->SubmitCommandBuffer(m_commandBuffers[m_vulkan->GetCurrentFrame()]);
    }

	
	auto RendererForward::OnObjectCreate( Message message ) -> void {
		auto handle = message.template GetData<MsgObjectCreate>().m_handle;
		auto [gHandle, tHandle] = m_registry.template Get<GeometryHandle, TextureHandle>(handle);

		decltype(auto) geometry = m_registry.template Get<vh::Geometry&>(gHandle);
		if( geometry.m_vertexBuffer == VK_NULL_HANDLE ) {
			vh::createVertexBuffer(m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_vulkan->GetVmaAllocator(), m_vulkan->GetGraphicsQueue(), m_commandPool, geometry);
			vh::createIndexBuffer( m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_vulkan->GetVmaAllocator(), m_vulkan->GetGraphicsQueue(), m_commandPool, geometry);
		}

		decltype(auto) texture = m_registry.template Get<vh::Texture&>(tHandle);
		if( texture.m_textureImage == VK_NULL_HANDLE && texture.m_pixels != nullptr ) {
			vh::createTextureImage(m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_vulkan->GetVmaAllocator(), m_vulkan->GetGraphicsQueue(), m_commandPool, texture.m_pixels, texture.m_width, texture.m_height, texture.m_size, texture);
			vh::createTextureImageView(m_vulkan->GetDevice(), texture);
			vh::createTextureSampler(m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), texture);
		}

		vh::UniformBuffers ubo;
		vh::createUniformBuffers(m_vulkan->GetPhysicalDevice(), m_vulkan->GetDevice(), m_vulkan->GetVmaAllocator(), ubo);

		vh::DescriptorSet descriptorSet;
		vh::createDescriptorSet(m_vulkan->GetDevice(), texture, m_descriptorSetLayouts, m_vulkan->GetDescriptorPool(), descriptorSet);
	    vh::updateDescriptorSetUBO(m_vulkan->GetDevice(), ubo, 0, descriptorSet);
	    vh::updateDescriptorSetTexture(m_vulkan->GetDevice(), texture, 1, descriptorSet);

		m_registry.template Put(handle, ubo, descriptorSet);

		assert( m_registry.template Has<vh::UniformBuffers>(handle) );
		assert( m_registry.template Has<vh::DescriptorSet>(handle) );
	}


    void RendererForward::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vulkan->GetDevice());
		
        vkDestroyCommandPool(m_vulkan->GetDevice(), m_commandPool, nullptr);
		vkDestroyPipeline(m_vulkan->GetDevice(), m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_vulkan->GetDevice(), m_graphicsPipeline.m_pipelineLayout, nullptr);        
		vkDestroyRenderPass(m_vulkan->GetDevice(), m_renderPass, nullptr);
		for( auto layout : m_descriptorSetLayouts.m_descriptorSetLayouts ) {
			vkDestroyDescriptorSetLayout(m_vulkan->GetDevice(), layout, nullptr);
		}
    }


};   // namespace vve


