#include "VHInclude.h"
#include "VEInclude.h"


/// Pipeline code:
/// P...Vertex data contains positions
/// N...Vertex data contains normals
/// U...Vertex data contains texture UV coordinates
/// T...Vertex data contains tangents
/// E...Object has texture map
/// C...Vertex data contains colors
/// O...Object has color in UBO
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

    RendererForward11::RendererForward11( std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName ) {

  		engine.RegisterCallback( { 
  			{this,  3500, "INIT", [this](Message& message){ return OnInit(message);} },
  			{this,  2000, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
  			{this,  2000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,  2000, "OBJECT_CREATE", [this](Message& message){ return OnObjectCreate(message);} },
			{this, 10000, "OBJECT_DESTROY", [this](Message& message){ return OnObjectDestroy(message);} },
  			{this,     0, "QUIT", [this](Message& message){ return OnQuit(message);} }
  		} );
    };

    RendererForward11::~RendererForward11(){};

    bool RendererForward11::OnInit(Message message) {
		Renderer::OnInit(message);

        vh::RenCreateRenderPass(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_swapChain, false, m_renderPass);
		
		vh::RenCreateDescriptorSetLayout( m_vulkanState().m_device, //Per frame
			{ 
				{ 	.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
				{ 	.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			m_descriptorSetLayoutPerFrame );

		vh::ComCreateCommandPool(m_vulkanState().m_surface, m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_commandPool);
		vh::ComCreateCommandBuffers(m_vulkanState().m_device, m_commandPool, m_commandBuffers);
		vh::RenCreateDescriptorPool(m_vulkanState().m_device, 1000, m_descriptorPool);

		vh::RenCreateDescriptorSet(m_vulkanState().m_device, m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetPerFrame);
		
		//Per frame uniform buffer
		vh::BufCreateBuffers(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(vh::UniformBufferFrame), m_uniformBuffersPerFrame);
		vh::RenUpdateDescriptorSet(m_vulkanState().m_device, m_uniformBuffersPerFrame, 0, 
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeof(vh::UniformBufferFrame), m_descriptorSetPerFrame);   

		//Per frame light buffer
		vh::BufCreateBuffers(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, 
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_maxNumberLights*sizeof(vh::Light), m_uniformBuffersLights);
		vh::RenUpdateDescriptorSet(m_vulkanState().m_device, m_uniformBuffersLights, 1, 
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_maxNumberLights*sizeof(vh::Light), m_descriptorSetPerFrame);   

		CreatePipelines();
		return false;
	}

	void RendererForward11::CreatePipelines() {
		const std::filesystem::path shaders{"shaders/Forward"};
		for( const auto& entry : std::filesystem::directory_iterator(shaders) ) {
			auto filename = entry.path().filename().string();
			if( filename.find("_vert.spv") != std::string::npos && std::isdigit(filename[0]) ) {
				size_t pos1 = filename.find("_");
				size_t pos2 = filename.find("_vert.spv");
				auto pri = std::stoi( filename.substr(0, pos1-1) );
				std::string type = filename.substr(pos1+1, pos2 - pos1 - 1);
				
				vh::Pipeline graphicsPipeline;

				VkDescriptorSetLayout descriptorSetLayoutPerObject;
				std::vector<VkDescriptorSetLayoutBinding> bindings{
					{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
				};

				if(type.find("U") != std::string::npos) { //texture map
					bindings.push_back( { .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT } );
				}

				vh::RenCreateDescriptorSetLayout( m_vulkanState().m_device, bindings, descriptorSetLayoutPerObject );

				std::vector<VkVertexInputBindingDescription> bindingDescriptions = getBindingDescriptions(type);
				std::vector<VkVertexInputAttributeDescription> attributeDescriptions = getAttributeDescriptions(type);

				vh::RenCreateGraphicsPipeline(m_vulkanState().m_device, m_renderPass, 
					entry.path().string(), (shaders / (filename.substr(0,pos2) + "_frag.spv")).string(),
					bindingDescriptions, attributeDescriptions,
					{ m_descriptorSetLayoutPerFrame, descriptorSetLayoutPerObject }, 
					{(int)m_maxNumberLights},
					graphicsPipeline);
				
				m_pipelinesPerType[pri] = { type, descriptorSetLayoutPerObject, graphicsPipeline };

				std::cout << "Pipeline (" << graphicsPipeline.m_pipeline << "): " << filename << " Priority: " << pri << " Type: " << type << std::endl;
			}
		}

	}

	void RendererForward11::getBindingDescription( std::string type, std::string C, int &binding, int stride, auto& bdesc ) {
		if( type.find(C) == std::string::npos ) return;
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = binding++;
		bindingDescription.stride = stride;
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bdesc.push_back( bindingDescription );
	}

	auto RendererForward11::getBindingDescriptions( std::string type ) -> std::vector<VkVertexInputBindingDescription> {
		std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
			
		int binding=0;
		getBindingDescription( type, "P", binding, size_pos, bindingDescriptions );
		getBindingDescription( type, "N", binding, size_nor, bindingDescriptions );
		getBindingDescription( type, "U", binding, size_tex, bindingDescriptions );
		getBindingDescription( type, "C", binding, size_col, bindingDescriptions );
		getBindingDescription( type, "T", binding, size_tan, bindingDescriptions );
		return bindingDescriptions;
	}

	void RendererForward11::addAttributeDescription( std::string type, std::string C, int& binding, int& location, VkFormat format, auto& attd ) {
		if( type.find(C) == std::string::npos ) return;
		VkVertexInputAttributeDescription attributeDescription{};
		attributeDescription.binding = binding++;
		attributeDescription.location = location++;
		attributeDescription.format = format;
		attributeDescription.offset = 0;
		attd.push_back( attributeDescription );
	}

    auto RendererForward11::getAttributeDescriptions(std::string type) -> std::vector<VkVertexInputAttributeDescription> {
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

	bool RendererForward11::OnPrepareNextFrame(Message message) {
		auto msg = message.template GetData<MsgPrepareNextFrame>();

		std::vector<vh::Light> lights{m_maxNumberLights};
		m_numberLightsPerType = glm::ivec3{0};
		size_t total{0};
		vh::UniformBufferFrame ubc; //contains camera view and projection matrices and number of lights
		for( auto [handle, light, lToW] : m_registry.template GetView<vecs::Handle, PointLight&, LocalToWorldMatrix&>() ) {
			++m_numberLightsPerType.x;
			light().params.x = 0.0f;
			lights[total] = { .positionW = glm::vec3{lToW()[3]}, .lightParams = light() };
			if( ++total >= m_maxNumberLights ) break;
		}
		for( auto [handle, light, lToW] : m_registry.template GetView<vecs::Handle, DirectionalLight&, LocalToWorldMatrix&>() ) {
			++m_numberLightsPerType.y;
			light().params.x = 1.0f;
			lights[total] = { .directionW = glm::vec3{lToW()[1]}, .lightParams = light() };
			if( ++total >= m_maxNumberLights ) break;
		}
		for( auto [handle, light, lToW] : m_registry.template GetView<vecs::Handle, SpotLight&, LocalToWorldMatrix&>() ) {
			++m_numberLightsPerType.z;
			light().params.x = 2.0f;
			lights[total] = { .positionW = glm::vec3{lToW()[3]}, .directionW = glm::vec3{lToW()[1]}, .lightParams = light() };
			if( ++total >= m_maxNumberLights ) break;
		}
		ubc.numLights = m_numberLightsPerType;
		memcpy(m_uniformBuffersLights.m_uniformBuffersMapped[m_vulkanState().m_currentFrame], lights.data(), total*sizeof(vh::Light));

		//Copy camera view and projection matrices to the uniform buffer
		auto [lToW, view, proj] = *m_registry.template GetView<LocalToWorldMatrix&, ViewMatrix&, ProjectionMatrix&>().begin();
		ubc.camera.view = view();
        ubc.camera.proj = proj();
		ubc.camera.positionW = lToW()[3];
		memcpy(m_uniformBuffersPerFrame.m_uniformBuffersMapped[m_vulkanState().m_currentFrame], &ubc, sizeof(ubc));
		return false;
	}

    bool RendererForward11::OnRecordNextFrame(Message message) {
		auto msg = message.template GetData<MsgRecordNextFrame>();

		vkResetCommandBuffer(m_commandBuffers[m_vulkanState().m_currentFrame],  0);
        
		vh::ComStartRecordCommandBuffer(m_commandBuffers[m_vulkanState().m_currentFrame], m_vulkanState().m_imageIndex, 
			m_vulkanState().m_swapChain, m_renderPass, false, 
			m_windowState().m_clearColor,  m_vulkanState().m_currentFrame);
		
		for( auto& pipeline : m_pipelinesPerType) {
			for( auto[oHandle, name, ghandle, LtoW, uniformBuffers, descriptorsets] : 
				m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vh::Buffer&, vh::DescriptorSet&>
						({(size_t)pipeline.second.m_graphicsPipeline.m_pipeline}) ) {

				bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
				bool hasColor = m_registry.template Has<vh::Color>(oHandle);
				bool hasVertexColor = pipeline.second.m_type.find("C") != std::string::npos;
				if( !hasTexture && !hasColor && !hasVertexColor ) continue;

				if( hasTexture ) {
					vh::BufferPerObjectTexture uboTexture{};
					uboTexture.model = LtoW(); 		
					uboTexture.modelInverseTranspose = glm::inverse( glm::transpose(uboTexture.model) );
					UVScale uvScale{ { 1.0f, 1.0f }};
					if( m_registry.template Has<UVScale>(oHandle) ) {
						uvScale = m_registry.template Get<UVScale>(oHandle);
					}
					uboTexture.uvScale = uvScale;
					memcpy(uniformBuffers().m_uniformBuffersMapped[m_vulkanState().m_currentFrame], &uboTexture, sizeof(uboTexture));
				} else if( hasColor ) {
					vh::BufferPerObjectColor uboColor{};
					uboColor.model = LtoW(); 		
					uboColor.modelInverseTranspose = glm::inverse( glm::transpose(uboColor.model) );
					uboColor.color = m_registry.template Get<vh::Color>(oHandle);
					memcpy(uniformBuffers().m_uniformBuffersMapped[m_vulkanState().m_currentFrame], &uboColor, sizeof(uboColor));
				} else if( hasVertexColor ) {
					vh::BufferPerObject uboColor{};
					uboColor.model = LtoW(); 
					uboColor.modelInverseTranspose = glm::inverse( glm::transpose(uboColor.model) );
					memcpy(uniformBuffers().m_uniformBuffersMapped[m_vulkanState().m_currentFrame], &uboColor, sizeof(uboColor));
				}

				vh::ComBindPipeline(m_commandBuffers[m_vulkanState().m_currentFrame], m_vulkanState().m_imageIndex, 
					m_vulkanState().m_swapChain, m_renderPass, pipeline.second.m_graphicsPipeline, false, 
					m_windowState().m_clearColor, 
					m_vulkanState().m_currentFrame);

				auto mesh = m_registry.template Get<vh::Mesh&>(ghandle);
				vh::ComRecordObject( m_commandBuffers[m_vulkanState().m_currentFrame], pipeline.second.m_graphicsPipeline, 
					{ m_descriptorSetPerFrame, descriptorsets }, pipeline.second.m_type, mesh, m_vulkanState().m_currentFrame );
			}
		}

		vh::ComEndRecordCommandBuffer(m_commandBuffers[m_vulkanState().m_currentFrame]);
	    SubmitCommandBuffer(m_commandBuffers[m_vulkanState().m_currentFrame]);
		return false;
    }

	bool RendererForward11::OnObjectCreate( Message message ) {
		ObjectHandle oHandle = message.template GetData<MsgObjectCreate>().m_object;
		assert( m_registry.template Has<MeshHandle>(oHandle) );	
		auto meshHandle = m_registry.template Get<MeshHandle>(oHandle);
		auto mesh = m_registry.template Get<vh::Mesh&>(meshHandle);
		auto type = getPipelineType(oHandle, mesh().m_verticesData);
		auto pipelinePerType = getPipelinePerType(type);

		bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
		bool hasColor = m_registry.template Has<vh::Color>(oHandle);
		bool hasVertexColor = pipelinePerType->m_type.find("C") != std::string::npos;
		if( !hasTexture && !hasColor && !hasVertexColor ) return false;	

		vh::Buffer ubo;
		size_t sizeUbo = 0;
		vh::DescriptorSet descriptorSet{1};
		vh::RenCreateDescriptorSet(m_vulkanState().m_device, pipelinePerType->m_descriptorSetLayoutPerObject, m_descriptorPool, descriptorSet);

		if( hasTexture ) {
			sizeUbo = sizeof(vh::BufferPerObjectTexture);
			auto tHandle = m_registry.template Get<TextureHandle>(oHandle);
			auto texture = m_registry.template Get<vh::Map&>(tHandle);
	    	vh::RenUpdateDescriptorSetTexture(m_vulkanState().m_device, texture, 1, descriptorSet);
		} else if(hasColor) {
			sizeUbo = sizeof(vh::BufferPerObjectColor);
		} else if(hasVertexColor) {
			sizeUbo = sizeof(vh::BufferPerObject);
		}

		vh::BufCreateBuffers(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeUbo, ubo);
	    vh::RenUpdateDescriptorSet(m_vulkanState().m_device, ubo, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeUbo, descriptorSet);

		m_registry.Put(oHandle, ubo, descriptorSet);
		m_registry.AddTags(oHandle, (size_t)pipelinePerType->m_graphicsPipeline.m_pipeline);

		assert( m_registry.template Has<vh::Buffer>(oHandle) );
		assert( m_registry.template Has<vh::DescriptorSet>(oHandle) );
		return false; //true if handled
	}

	bool RendererForward11::OnObjectDestroy( Message message ) {
		auto msg = message.template GetData<MsgObjectDestroy>();
		auto oHandle = msg.m_handle();

		assert(m_registry.Exists(oHandle) );

		if( !m_registry.template Has<vh::Buffer>(oHandle) ) return false;
		auto ubo = m_registry.template Get<vh::Buffer&>(oHandle);
		vh::BufDestroyBuffer2(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, ubo);
		return false;
	}


	/// @brief 
	/// @param handle 
	/// @param vertexData 
	/// @return 
	std::string RendererForward11::getPipelineType(ObjectHandle handle, vh::VertexData &vertexData) {
		std::string type = vertexData.getType();
		if( m_registry.template Has<TextureHandle>(handle) && type.find("U") != std::string::npos ) type += "E";
		if( m_registry.template Has<vh::Color>(handle) && type.find("C") == std::string::npos && type.find("E") == std::string::npos ) type += "O";
		return type;
	}

	RendererForward11::PipelinePerType* RendererForward11::getPipelinePerType(std::string type) {
		for( auto& [pri, pipeline] : m_pipelinesPerType ) {
			bool found = true;
			for( auto& c : pipeline.m_type ) {found = found && ( type.find(c) != std::string::npos ); }
			if( found ) return &pipeline;
		}
		std::cout << "Pipeline not found for type: " << type << std::endl;
		exit(-1);
		return nullptr;
	}

    bool RendererForward11::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vulkanState().m_device);
		
        vkDestroyCommandPool(m_vulkanState().m_device, m_commandPool, nullptr);

		for( auto& [type, pipeline] : m_pipelinesPerType ) {
			vkDestroyDescriptorSetLayout(m_vulkanState().m_device, pipeline.m_descriptorSetLayoutPerObject, nullptr);
			vkDestroyPipeline(m_vulkanState().m_device, pipeline.m_graphicsPipeline.m_pipeline, nullptr);
			vkDestroyPipelineLayout(m_vulkanState().m_device, pipeline.m_graphicsPipeline.m_pipelineLayout, nullptr);
		}

        vkDestroyDescriptorPool(m_vulkanState().m_device, m_descriptorPool, nullptr);
		vkDestroyRenderPass(m_vulkanState().m_device, m_renderPass, nullptr);
		vh::BufDestroyBuffer2(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_uniformBuffersPerFrame);
		vh::BufDestroyBuffer2(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_uniformBuffersLights);
		vkDestroyDescriptorSetLayout(m_vulkanState().m_device, m_descriptorSetLayoutPerFrame, nullptr);

		return false;
    }


};   // namespace vve


