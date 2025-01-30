#include <filesystem>

#include "VHInclude.h"
#include "VEInclude.h"


/// Pipeline code:
/// P...Vertex data contains positions
/// N...Vertex data contains normals
/// T...Vertex data contains tangents
/// C...Vertex data contains colors
/// U...Vertex data contains texture UV coordinates
/// O...Object has color in UBO
/// E...Object has texture map
/// R...Object has normal map
/// S...Object has specular map
/// M...Object has metallic map
/// A...Object has roughness map
/// D...Object has displacement map
/// F...Object has ambient occlusion map
/// L...Object has emissive map
/// B...Object has brdf map
/// H...Object has height map
/// P...Object is transparent

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
        vh::RenCreateRenderPass(GetPhysicalDevice(), GetDevice(), GetSwapChain(), false, m_renderPass);
		
		vh::RenCreateDescriptorSetLayout( GetDevice(), //Per frame
			{{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT } },
			m_descriptorSetLayoutPerFrame );

        vh::ComCreateCommandPool(GetSurface(), GetPhysicalDevice(), GetDevice(), m_commandPool);
        vh::ComCreateCommandBuffers(GetDevice(), m_commandPool, m_commandBuffers);
        vh::RenCreateDescriptorPool(GetDevice(), 1000, m_descriptorPool);

		vh::BufCreateUniformBuffers(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), sizeof(vh::UniformBufferFrame), m_uniformBuffersPerFrame);
		vh::RenCreateDescriptorSet(GetDevice(), m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetPerFrame);
	    vh::RenUpdateDescriptorSetUBO(GetDevice(), m_uniformBuffersPerFrame, 0, sizeof(vh::UniformBufferFrame), m_descriptorSetPerFrame);   

		const std::filesystem::path shaders{"shaders\\Forward"};
		for( const auto& entry : std::filesystem::directory_iterator(shaders) ) {
			auto filename = entry.path().filename().string();
			if( filename.find("_vert.spv") != std::string::npos && filename[0] == 'S' ) {
				size_t pos1 = filename.find("_");
				size_t pos2 = filename.find("_vert.spv");
				auto pri = std::stoi( filename.substr(1, pos1-1) );
				std::string type = filename.substr(pos1+1, pos2 - pos1 - 1);

				std::cout << "Pipeline: " << filename << " Priority: " << pri << " Type: " << type << std::endl;
				
				vh::Pipeline graphicsPipeline;

				VkDescriptorSetLayout descriptorSetLayoutPerObject;
				std::vector<VkDescriptorSetLayoutBinding> bindings{
					{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
				};

				if(type.find("U") != std::string::npos) { //texture map
					bindings.push_back( { .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT } );
				}

				vh::RenCreateDescriptorSetLayout( GetDevice(), bindings, descriptorSetLayoutPerObject );

				std::vector<VkVertexInputBindingDescription> bindingDescriptions = getBindingDescriptions(type);
				std::vector<VkVertexInputAttributeDescription> attributeDescriptions = getAttributeDescriptions(type);

				vh::RenCreateGraphicsPipeline(GetDevice(), m_renderPass, 
					entry.path().string(), (shaders / (filename.substr(0,pos2) + "_frag.spv")).string(),
					bindingDescriptions, attributeDescriptions,
					{ m_descriptorSetLayoutPerFrame, descriptorSetLayoutPerObject }, graphicsPipeline);
				
				m_pipelinesPerType[pri] = { type, descriptorSetLayoutPerObject, graphicsPipeline };
			}
		}

		return false;
	}

	void RendererForward::getBindingDescription( std::string type, std::string C, int &binding, int stride, auto& bdesc ) {
		if( type.find(C) == std::string::npos ) return;
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = binding++;
		bindingDescription.stride = stride;
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bdesc.push_back( bindingDescription );
	}

	auto RendererForward::getBindingDescriptions( std::string type ) -> std::vector<VkVertexInputBindingDescription> {
		std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
			
		int binding=0;
		getBindingDescription( type, "P", binding, size_pos, bindingDescriptions );
		getBindingDescription( type, "N", binding, size_nor, bindingDescriptions );
		getBindingDescription( type, "U", binding, size_tex, bindingDescriptions );
		getBindingDescription( type, "C", binding, size_col, bindingDescriptions );
		getBindingDescription( type, "T", binding, size_tan, bindingDescriptions );
		return bindingDescriptions;
	}

	void RendererForward::addAttributeDescription( std::string type, std::string C, int& binding, int& location, VkFormat format, auto& attd ) {
		if( type.find(C) == std::string::npos ) return;
		VkVertexInputAttributeDescription attributeDescription{};
		attributeDescription.binding = binding++;
		attributeDescription.location = location++;
		attributeDescription.format = format;
		attributeDescription.offset = 0;
		attd.push_back( attributeDescription );
	}

    auto RendererForward::getAttributeDescriptions(std::string type) -> std::vector<VkVertexInputAttributeDescription> {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

		int binding=0;
		int location=0;
		addAttributeDescription( type, "P", binding, location, VK_FORMAT_R32G32B32_SFLOAT, attributeDescriptions );
		addAttributeDescription( type, "N", binding, location, VK_FORMAT_R32G32B32_SFLOAT, attributeDescriptions );
		addAttributeDescription( type, "U", binding, location, VK_FORMAT_R32G32_SFLOAT,    attributeDescriptions );
		addAttributeDescription( type, "C", binding, location, VK_FORMAT_R32G32B32A32_SFLOAT, attributeDescriptions );
		addAttributeDescription( type, "T", binding, location, VK_FORMAT_R32G32B32_SFLOAT, attributeDescriptions );
        return attributeDescriptions;
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
        
		vh::ComStartRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
			GetSwapChain(), m_renderPass, m_graphicsPipeline, false, ((WindowSDL*)m_window)->GetClearColor(), GetCurrentFrame());
		
		for( auto& pipeline : m_pipelinesPerType) {
			for( auto[handle, name, ghandle, LtoW, uniformBuffers, descriptorsets] : 
				m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vh::UniformBuffers&, 
					vh::DescriptorSet&>({(size_t)pipeline.second.m_graphicsPipeline.m_pipeline}) ) {

				vh::UniformBufferObject ubo{};
				ubo.model = LtoW(); 		
				ubo.modelInverseTranspose = glm::inverse( glm::transpose(ubo.model) );
				if( m_registry.template Has<vh::Color>(ghandle) ) {
					auto color = m_registry.template Get<vh::Color&>(ghandle);
					ubo.color = color;
				}

				memcpy(uniformBuffers.m_uniformBuffersMapped[GetCurrentFrame()], &ubo, sizeof(ubo));
				vh::Mesh& mesh = m_registry.template Get<vh::Mesh&>(ghandle);

				vh::ComBindPipeline(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
					GetSwapChain(), m_renderPass, pipeline.second.m_graphicsPipeline, false, 
					((WindowSDL*)m_window)->GetClearColor(), GetCurrentFrame());

				vh::ComRecordObject( m_commandBuffers[GetCurrentFrame()], pipeline.second.m_graphicsPipeline, 
					{ m_descriptorSetPerFrame, descriptorsets }, pipeline.second.m_type, mesh, GetCurrentFrame() );
			}
		}

		vh::ComEndRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
	    SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
		return false;
    }

	bool RendererForward::OnObjectCreate( Message message ) {
		ObjectHandle handle = message.template GetData<MsgObjectCreate>().m_object;
		auto gHandle = m_registry.template Get<MeshHandle>(handle);
		auto mesh = m_registry.template Get<vh::Mesh&>(gHandle);
		auto pipelinePerType = getPipelinePerType(getPipelineType(handle, mesh.m_verticesData));

		vh::UniformBuffers ubo;
		vh::BufCreateUniformBuffers(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), sizeof(vh::UniformBufferObject), ubo);

		vh::DescriptorSet descriptorSet{1};
		vh::RenCreateDescriptorSet(GetDevice(), pipelinePerType->m_descriptorSetLayoutPerObject, m_descriptorPool, descriptorSet);
	    vh::RenUpdateDescriptorSetUBO(GetDevice(), ubo, 0, sizeof(vh::UniformBufferObject), descriptorSet);

		if( m_registry.template Has<TextureHandle>(handle) ) {
			auto tHandle = m_registry.template Get<TextureHandle>(handle);
			auto& texture = m_registry.template Get<vh::Texture&>(tHandle);
	    	vh::RenUpdateDescriptorSetTexture(GetDevice(), texture, 1, descriptorSet);
		}
		m_registry.Put(handle, ubo, descriptorSet);
		m_registry.template AddTags(handle, (size_t)pipelinePerType->m_graphicsPipeline.m_pipeline);

		assert( m_registry.template Has<vh::UniformBuffers>(handle) );
		assert( m_registry.template Has<vh::DescriptorSet>(handle) );
		return false; //true if handled
	}


	/// @brief 
	/// @param handle 
	/// @param vertexData 
	/// @return 
	std::string RendererForward::getPipelineType(ObjectHandle handle, vh::VertexData &vertexData) {
		std::string type = vertexData.getType();
		if( m_registry.template Has<vh::Color>(handle) && type.find("C") == std::string::npos && type.find("U") == std::string::npos ) type += "O";
		return type;
	}

	RendererForward::PipelinePerType* RendererForward::getPipelinePerType(std::string type) {
		for( auto& [pri, pipeline] : m_pipelinesPerType ) {
			bool found = true;
			for( auto& c : pipeline.m_type ) {found = found && ( type.find(c) != std::string::npos ); }
			if( found ) return &pipeline;
		}
		std::cout << "Pipeline not found for type: " << type << std::endl;
		exit(-1);
		return nullptr;
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

		vh::BufDestroyBuffer2(GetDevice(), GetVmaAllocator(), m_uniformBuffersPerFrame);

		vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayoutPerFrame, nullptr);
		return false;
    }


};   // namespace vve


