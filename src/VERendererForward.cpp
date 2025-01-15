
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {

    RendererForward::RendererForward( std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName ) {

  		engine.RegisterCallback( { 
  			{this,  3000, "INIT", [this](Message& message){ return OnInit(message);} },
  			{this,  2000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,  2000, "OBJECT_CREATE", [this](Message& message){ return OnObjectCreate(message);} },
  			{this,     0, "QUIT", [this](Message& message){ return OnQuit(message);} }
  		} );
    };

    RendererForward::~RendererForward(){};

    bool RendererForward::OnInit(Message message) {
        vh::createRenderPass(GetPhysicalDevice(), GetDevice(), GetSwapChain(), false, m_renderPass);
		
		vh::createDescriptorSetLayout( GetDevice(), //Per object
			{{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			 {.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT } },
			m_descriptorSetLayoutPerObject );

		vh::createDescriptorSetLayout( GetDevice(), //Per frame
			{{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT } },
			m_descriptorSetLayoutPerFrame );

		vh::createGraphicsPipeline(GetDevice(), m_renderPass, "shaders\\Forward\\vert.spv", "shaders\\Forward\\frag.spv", 
			{ m_descriptorSetLayoutPerFrame, m_descriptorSetLayoutPerObject }, m_graphicsPipeline);

        vh::createCommandPool(GetSurface(), GetPhysicalDevice(), GetDevice(), m_commandPool);
        vh::createCommandBuffers(GetDevice(), GetCommandPool(), m_commandBuffers);

		vh::createUniformBuffers(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), sizeof(UniformBufferFrame), m_uniformBuffersPerFrame);
		vh::createDescriptorSet(GetDevice(), m_descriptorSetLayoutPerFrame, GetDescriptorPool(), m_descriptorSetPerFrame);
	    vh::updateDescriptorSetUBO(GetDevice(), m_uniformBuffersPerFrame, 0, sizeof(UniformBufferFrame), m_descriptorSetPerFrame);   
		return false;
	}

    bool RendererForward::OnRecordNextFrame(Message message) {
		static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		auto [view, proj] = *m_registry.template GetView<ViewMatrix&, ProjectionMatrix&>().begin();
		UniformBufferFrame ubc;
		ubc.view = view;
        ubc.proj = proj;
		memcpy(m_uniformBuffersPerFrame.m_uniformBuffersMapped[GetCurrentFrame()], &ubc, sizeof(ubc));

		vkResetCommandBuffer(m_commandBuffers[GetCurrentFrame()],  0);
        
		vh::startRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
			GetSwapChain(), m_renderPass, m_graphicsPipeline, false, ((WindowSDL*)m_window)->GetClearColor(), GetCurrentFrame());
		
		for( auto[name, ghandle, LtoW, uniformBuffers, descriptorsets] : m_registry.template GetView<Name, GeometryHandle, LocalToWorldMatrix&, vh::UniformBuffers&, vh::DescriptorSet&>() ) {
			UniformBufferObject ubo{};
			ubo.model = LtoW(); // * glm::rotate(glm::mat4(1.0f), time * glm::radians(50.0f), glm::vec3(0.0f, 0.0f, 1.0f));			
			memcpy(uniformBuffers.m_uniformBuffersMapped[GetCurrentFrame()], &ubo, sizeof(ubo));
			vh::Geometry& geometry = m_registry.template Get<vh::Geometry&>(ghandle);
			vh::recordObject2( m_commandBuffers[GetCurrentFrame()], m_graphicsPipeline, { descriptorsets, m_descriptorSetPerFrame }, geometry, GetCurrentFrame() );
		}

		vh::endRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
	    SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
		return false;
    }

	
	bool RendererForward::OnObjectCreate( Message message ) {
		auto handle = message.template GetData<MsgObjectCreate>().m_object;
		auto [gHandle, tHandle] = m_registry.template Get<GeometryHandle, TextureHandle>(handle);
		auto& texture = m_registry.template Get<vh::Texture&>(tHandle);

		vh::UniformBuffers ubo;
		vh::createUniformBuffers(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), sizeof(UniformBufferObject), ubo);

		vh::DescriptorSet descriptorSet{1};
		vh::createDescriptorSet(GetDevice(), m_descriptorSetLayoutPerObject, GetDescriptorPool(), descriptorSet);
	    vh::updateDescriptorSetUBO(GetDevice(), ubo, 0, sizeof(UniformBufferObject), descriptorSet);
	    vh::updateDescriptorSetTexture(GetDevice(), texture, 1, descriptorSet);

		m_registry.Put(handle, ubo, descriptorSet);

		assert( m_registry.template Has<vh::UniformBuffers>(handle) );
		assert( m_registry.template Has<vh::DescriptorSet>(handle) );
		return false; //true if handled
	}


    bool RendererForward::OnQuit(Message message) {
        vkDeviceWaitIdle(GetDevice());
		
        vkDestroyCommandPool(GetDevice(), GetCommandPool(), nullptr);
		vkDestroyPipeline(GetDevice(), m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(GetDevice(), m_graphicsPipeline.m_pipelineLayout, nullptr);        
		vkDestroyRenderPass(GetDevice(), m_renderPass, nullptr);

		vh::destroyBuffer2(GetDevice(), GetVmaAllocator(), m_uniformBuffersPerFrame);

		vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayoutPerFrame, nullptr);
		vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayoutPerObject, nullptr);
		return false;
    }


};   // namespace vve


