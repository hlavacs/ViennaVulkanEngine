
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
        vh::createRenderPass(GetPhysicalDevice(), GetDevice(), GetSwapChain(), false, m_renderPass);
		
		vh::createDescriptorSetLayout(GetDevice(),
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

		vh::createGraphicsPipeline(GetDevice(), m_renderPass, m_descriptorSetLayouts, m_graphicsPipeline);

        vh::createCommandPool(GetSurface(), GetPhysicalDevice(), GetDevice(), m_commandPool);
        vh::createCommandBuffers(GetDevice(), m_commandPool, m_commandBuffers);
    }

    void RendererForward::OnRecordNextFrame(Message message) {
		
		for( decltype(auto) ubo : m_registry.template GetView<vh::UniformBuffers&>() ) {
        	vh::updateUniformBuffer(GetCurrentFrame(), GetSwapChain(), ubo);
		}

		vkResetCommandBuffer(m_commandBuffers[GetCurrentFrame()],  0);
        
		vh::startRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
			GetSwapChain(), m_renderPass, m_graphicsPipeline, 
			false, ((WindowSDL*)m_window)->GetClearColor(), GetCurrentFrame());
		
		for( auto[ghandle, ubo, descriptorsets] : m_registry.template GetView<GeometryHandle, vh::UniformBuffers&, vh::DescriptorSet&>() ) {
			auto& geometry = m_registry.template Get<vh::Geometry&>(ghandle);
			vh::recordObject2( m_commandBuffers[GetCurrentFrame()], m_graphicsPipeline, descriptorsets, geometry, GetCurrentFrame() );
		}

		vh::endRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()]);

	    SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
    }

	
	auto RendererForward::OnObjectCreate( Message message ) -> void {
		auto handle = message.template GetData<MsgObjectCreate>().m_handle;
		auto [gHandle, tHandle] = m_registry.template Get<GeometryHandle, TextureHandle>(handle);

		decltype(auto) geometry = m_registry.template Get<vh::Geometry&>(gHandle);
		if( geometry.m_vertexBuffer == VK_NULL_HANDLE ) {
			vh::createVertexBuffer(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetGraphicsQueue(), m_commandPool, geometry);
			vh::createIndexBuffer( GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetGraphicsQueue(), m_commandPool, geometry);
		}

		decltype(auto) texture = m_registry.template Get<vh::Texture&>(tHandle);
		if( texture.m_textureImage == VK_NULL_HANDLE && texture.m_pixels != nullptr ) {
			vh::createTextureImage(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetGraphicsQueue(), m_commandPool, texture.m_pixels, texture.m_width, texture.m_height, texture.m_size, texture);
			vh::createTextureImageView(GetDevice(), texture);
			vh::createTextureSampler(GetPhysicalDevice(), GetDevice(), texture);
		}

		vh::UniformBuffers ubo;
		vh::createUniformBuffers(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), ubo);

		vh::DescriptorSet descriptorSet;
		vh::createDescriptorSet(GetDevice(), texture, m_descriptorSetLayouts, GetDescriptorPool(), descriptorSet);
	    vh::updateDescriptorSetUBO(GetDevice(), ubo, 0, descriptorSet);
	    vh::updateDescriptorSetTexture(GetDevice(), texture, 1, descriptorSet);

		m_registry.template Put(handle, ubo, descriptorSet);

		assert( m_registry.template Has<vh::UniformBuffers>(handle) );
		assert( m_registry.template Has<vh::DescriptorSet>(handle) );
	}


    void RendererForward::OnQuit(Message message) {
        vkDeviceWaitIdle(GetDevice());
		
        vkDestroyCommandPool(GetDevice(), m_commandPool, nullptr);
		vkDestroyPipeline(GetDevice(), m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(GetDevice(), m_graphicsPipeline.m_pipelineLayout, nullptr);        
		vkDestroyRenderPass(GetDevice(), m_renderPass, nullptr);
		for( auto layout : m_descriptorSetLayouts.m_descriptorSetLayouts ) {
			vkDestroyDescriptorSetLayout(GetDevice(), layout, nullptr);
		}
    }


};   // namespace vve


