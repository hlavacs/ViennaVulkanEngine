#include <filesystem>

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
		
		vh::createDescriptorSetLayout( GetDevice(), //Per frame
			{{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT } },
			m_descriptorSetLayoutPerFrame );

        vh::createCommandPool(GetSurface(), GetPhysicalDevice(), GetDevice(), m_commandPool);
        vh::createCommandBuffers(GetDevice(), m_commandPool, m_commandBuffers);
        vh::createDescriptorPool(GetDevice(), 1000, m_descriptorPool);

		vh::createUniformBuffers(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), sizeof(vh::UniformBufferFrame), m_uniformBuffersPerFrame);
		vh::createDescriptorSet(GetDevice(), m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetPerFrame);
	    vh::updateDescriptorSetUBO(GetDevice(), m_uniformBuffersPerFrame, 0, sizeof(vh::UniformBufferFrame), m_descriptorSetPerFrame);   
		return false;
	}

    bool RendererForward::OnRecordNextFrame(Message message) {
		static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		auto [view, proj] = *m_registry.template GetView<ViewMatrix&, ProjectionMatrix&>().begin();
		vh::UniformBufferFrame ubc;
		ubc.view = view;
        ubc.proj = proj;
		memcpy(m_uniformBuffersPerFrame.m_uniformBuffersMapped[GetCurrentFrame()], &ubc, sizeof(ubc));

		vkResetCommandBuffer(m_commandBuffers[GetCurrentFrame()],  0);
        
		vh::startRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
			GetSwapChain(), m_renderPass, m_graphicsPipeline, false, ((WindowSDL*)m_window)->GetClearColor(), GetCurrentFrame());
		
		for( auto[handle, name, ghandle, LtoW, uniformBuffers, descriptorsets] : m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vh::UniformBuffers&, vh::DescriptorSet&>() ) {
			vh::UniformBufferObject ubo{};
			ubo.model = LtoW(); 		
			ubo.modelInverseTranspose = glm::inverse( glm::transpose(ubo.model) );
			if( m_registry.template Has<vh::Color>(ghandle) ) {
				auto color = m_registry.template Get<vh::Color&>(ghandle);
				ubo.color = color;
			}

			memcpy(uniformBuffers.m_uniformBuffersMapped[GetCurrentFrame()], &ubo, sizeof(ubo));
			vh::Mesh& mesh = m_registry.template Get<vh::Mesh&>(ghandle);
			auto pipelinePerType = getPipelinePerType(getPipelineType(ObjectHandle{handle}, mesh.m_verticesData), mesh.m_verticesData);

			vh::bindPipeline(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
				GetSwapChain(), m_renderPass, pipelinePerType.m_graphicsPipeline, false, ((WindowSDL*)m_window)->GetClearColor(), GetCurrentFrame());
		
			vh::recordObject2( m_commandBuffers[GetCurrentFrame()], pipelinePerType.m_graphicsPipeline, { descriptorsets, m_descriptorSetPerFrame }, mesh, GetCurrentFrame() );
		}

		vh::endRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
	    SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
		return false;
    }

	bool RendererForward::OnObjectCreate( Message message ) {
		ObjectHandle handle = message.template GetData<MsgObjectCreate>().m_object;
		auto gHandle = m_registry.template Get<MeshHandle>(handle);
		auto mesh = m_registry.template Get<vh::Mesh&>(gHandle);
		auto pipelinePerType = getPipelinePerType(getPipelineType(handle, mesh.m_verticesData), mesh.m_verticesData);

		vh::UniformBuffers ubo;
		vh::createUniformBuffers(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), sizeof(vh::UniformBufferObject), ubo);

		vh::DescriptorSet descriptorSet{1};
		vh::createDescriptorSet(GetDevice(), pipelinePerType.m_descriptorSetLayoutPerObject, m_descriptorPool, descriptorSet);
	    vh::updateDescriptorSetUBO(GetDevice(), ubo, 0, sizeof(vh::UniformBufferObject), descriptorSet);

		if( m_registry.template Has<TextureHandle>(handle) ) {
			auto tHandle = m_registry.template Get<TextureHandle>(handle);
			auto& texture = m_registry.template Get<vh::Texture&>(tHandle);
	    	vh::updateDescriptorSetTexture(GetDevice(), texture, 1, descriptorSet);
		}
		m_registry.Put(handle, ubo, descriptorSet);

		assert( m_registry.template Has<vh::UniformBuffers>(handle) );
		assert( m_registry.template Has<vh::DescriptorSet>(handle) );
		return false; //true if handled
	}

	std::string RendererForward::getPipelineType(ObjectHandle handle, vh::VertexData &vertexData) {
		std::string type = vertexData.getType();
		if( m_registry.template Has<vh::Color>(handle) && type.find("U") == std::string::npos ) type += "O";
		return type;
	}

	RendererForward::PipelinePerType& RendererForward::getPipelinePerType(std::string type, vh::VertexData &vertexData) {
		if( m_pipelinesPerType.contains(type) ) return m_pipelinesPerType[type];

		VkDescriptorSetLayout descriptorSetLayoutPerObject;
		std::vector<VkDescriptorSetLayoutBinding> bindings{
			{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
		};

		if(type.find("U") != std::string::npos) {
			bindings.push_back( { .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT } );
		}

		vh::createDescriptorSetLayout( GetDevice(), bindings, descriptorSetLayoutPerObject );

		vh::Pipeline graphicsPipeline;
		std::filesystem::path vertShaderPath = "shaders\\Forward\\" + type + "_vert.spv";
		std::filesystem::path fragShaderPath = "shaders\\Forward\\" + type + "_frag.spv";
		vh::createGraphicsPipeline2(GetDevice(), m_renderPass, vertShaderPath.string(), fragShaderPath.string(), 
			vertexData,
			{ m_descriptorSetLayoutPerFrame, descriptorSetLayoutPerObject }, graphicsPipeline);

		return m_pipelinesPerType[type] = { type, descriptorSetLayoutPerObject, graphicsPipeline };
	}

    bool RendererForward::OnQuit(Message message) {
        vkDeviceWaitIdle(GetDevice());
		
        vkDestroyCommandPool(GetDevice(), m_commandPool, nullptr);

		for( auto& [type, pipeline] : m_pipelinesPerType ) {
			vkDestroyDescriptorSetLayout(GetDevice(), pipeline.m_descriptorSetLayoutPerObject, nullptr);
			vkDestroyPipeline(GetDevice(), pipeline.m_graphicsPipeline.m_pipeline, nullptr);
			vkDestroyPipelineLayout(GetDevice(), pipeline.m_graphicsPipeline.m_pipelineLayout, nullptr);
		}

        vkDestroyDescriptorPool(GetDevice(), m_descriptorPool, nullptr);
		vkDestroyRenderPass(GetDevice(), m_renderPass, nullptr);

		vh::destroyBuffer2(GetDevice(), GetVmaAllocator(), m_uniformBuffersPerFrame);

		vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayoutPerFrame, nullptr);
		return false;
    }


};   // namespace vve


