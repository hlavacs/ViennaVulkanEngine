#include "VHInclude2.h"
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

  		engine.RegisterCallbacks( { 
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

        vvh::RenCreateRenderPass({
			.m_depthFormat 	= m_vkState().m_depthMapFormat, 
			.m_device 		= m_vkState().m_device, 
			.m_swapChain 	= m_vkState().m_swapChain, 
			.m_clear 		= true, 
			.m_renderPass 	= m_renderPassClear
		});

        vvh::RenCreateRenderPass({
			.m_depthFormat 	= m_vkState().m_depthMapFormat, 
			.m_device 		= m_vkState().m_device, 
			.m_swapChain 	= m_vkState().m_swapChain, 
			.m_clear 		= false, 
			.m_renderPass 	= m_renderPass
		});
		
		vvh::RenCreateDescriptorSetLayout( {
			.m_device = m_vkState().m_device, //Per frame
			.m_bindings = { 
				{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
				{ .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			.m_descriptorSetLayout = m_descriptorSetLayoutPerFrame 
		});

		for( int i=0; i<MAX_FRAMES_IN_FLIGHT; ++i) {
			m_commandPools.resize(MAX_FRAMES_IN_FLIGHT);
			vvh::ComCreateCommandPool({
				.m_surface 			= m_vkState().m_surface, 
				.m_physicalDevice 	= m_vkState().m_physicalDevice, 
				.m_device 			= m_vkState().m_device, 
				.m_commandPool 		= m_commandPools[i]
			});
		}

		vvh::RenCreateDescriptorPool({
			.m_device 			= m_vkState().m_device, 
			.m_sizes 			= 1000, 
			.m_descriptorPool 	= m_descriptorPool
		});
		vvh::RenCreateDescriptorSet({
			.m_device 				= m_vkState().m_device, 
			.m_descriptorSetLayouts	= m_descriptorSetLayoutPerFrame, 
			.m_descriptorPool 		= m_descriptorPool, 
			.m_descriptorSet 		= m_descriptorSetPerFrame
		});
		
		// -----------------------------------------------------------------------------------------------

		//Per frame uniform buffer
		vvh::BufCreateBuffers({
			m_vkState().m_device, 
			m_vkState().m_vmaAllocator, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			sizeof(vvh::UniformBufferFrame), 
			m_uniformBuffersPerFrame
		});
		vvh::RenUpdateDescriptorSet({
			m_vkState().m_device, 
			m_uniformBuffersPerFrame, 0, 
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
			sizeof(vvh::UniformBufferFrame), 
			m_descriptorSetPerFrame
		});   

		//Per frame light storage buffer
		vvh::BufCreateBuffers({
			m_vkState().m_device, 
			m_vkState().m_vmaAllocator, 
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 
			MAX_NUMBER_LIGHTS*sizeof(vvh::Light), 
			m_storageBuffersLights
		});
		vvh::RenUpdateDescriptorSet({
			m_vkState().m_device, 
			m_storageBuffersLights, 
			1, 
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
			MAX_NUMBER_LIGHTS*sizeof(vvh::Light), 
			m_descriptorSetPerFrame
		});   

		// -----------------------------------------------------------------------------------------------

		CreatePipelines();
		return false;
	}

	void RendererForward11::CreatePipelines() {
		const std::filesystem::path shaders{"shaders/Forward"};
		for( const auto& entry : std::filesystem::directory_iterator(shaders) ) {
			auto filename = entry.path().filename().string();
			if( filename.find(".spv") != std::string::npos && std::isdigit(filename[0]) ) {
				size_t pos1 = filename.find("_");
				size_t pos2 = filename.find(".spv");
				auto pri = std::stoi( filename.substr(0, pos1-1) );
				std::string type = filename.substr(pos1+1, pos2 - pos1 - 1);
				
				vvh::Pipeline graphicsPipeline;

				VkDescriptorSetLayout descriptorSetLayoutPerObject;
				std::vector<VkDescriptorSetLayoutBinding> bindings{
					{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
				};

				if(type.find("U") != std::string::npos) { //texture map
					bindings.push_back( { .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT } );
				}

				vvh::RenCreateDescriptorSetLayout( {m_vkState().m_device, bindings, descriptorSetLayoutPerObject });

				std::vector<VkVertexInputBindingDescription> bindingDescriptions = getBindingDescriptions(type);
				std::vector<VkVertexInputAttributeDescription> attributeDescriptions = getAttributeDescriptions(type);

				VkPipelineColorBlendAttachmentState colorBlendAttachment{};
				colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
				colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
				colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
				colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
				colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
				colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
				colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_MAX;
				colorBlendAttachment.blendEnable = VK_TRUE;

				vvh::RenCreateGraphicsPipeline({
					m_vkState().m_device, 
					m_renderPass, 
					entry.path().string(), 
					entry.path().string(),
					bindingDescriptions, 
					attributeDescriptions,
					{ m_descriptorSetLayoutPerFrame, 
						descriptorSetLayoutPerObject }, 
					{(int)MAX_NUMBER_LIGHTS}, //spezialization constants
					{{.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = 8}}, //push constant ranges -> 2 ints
					{colorBlendAttachment}, //blend attachments
					graphicsPipeline
				});
				
				m_pipelinesPerType[pri] = { type, descriptorSetLayoutPerObject, graphicsPipeline };

				std::cout << "Pipeline (" << graphicsPipeline.m_pipeline << "): " << filename << " Priority: " << pri << " Type: " << type << std::endl;
			}
		}
	}

	bool RendererForward11::OnPrepareNextFrame(Message message) {
		auto msg = message.template GetData<MsgPrepareNextFrame>();

		m_pass = 0;
		std::vector<vvh::Light> lights{MAX_NUMBER_LIGHTS};
		m_numberLightsPerType = glm::ivec3{0};
		int total{0};
		vvh::UniformBufferFrame ubc; //contains camera view and projection matrices and number of lights
		vkResetCommandPool( m_vkState().m_device, m_commandPools[m_vkState().m_currentFrame], 0);

		//m_engine.DeregisterCallbacks(this, "RECORD_NEXT_FRAME");

		m_numberLightsPerType.x = RegisterLight<PointLight>(1.0f, lights, total);
		m_numberLightsPerType.y = RegisterLight<DirectionalLight>(2.0f, lights, total);
		m_numberLightsPerType.z = RegisterLight<SpotLight>(3.0f, lights, total);
		ubc.numLights = m_numberLightsPerType;
		memcpy(m_storageBuffersLights.m_uniformBuffersMapped[m_vkState().m_currentFrame], lights.data(), total*sizeof(vvh::Light));

		//Copy camera view and projection matrices to the uniform buffer
		auto [lToW, view, proj] = *m_registry.template GetView<LocalToWorldMatrix&, ViewMatrix&, ProjectionMatrix&>().begin();
		ubc.camera.view = view();
        ubc.camera.proj = proj();
		ubc.camera.positionW = lToW()[3];
		memcpy(m_uniformBuffersPerFrame.m_uniformBuffersMapped[m_vkState().m_currentFrame], &ubc, sizeof(ubc));

		for( auto& pipeline : m_pipelinesPerType) {
			for( auto[oHandle, name, ghandle, LtoW, uniformBuffers] : 
				m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vvh::Buffer&>
						({(size_t)pipeline.second.m_graphicsPipeline.m_pipeline}) ) {

				bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
				bool hasColor = m_registry.template Has<vvh::Color>(oHandle);
				bool hasVertexColor = pipeline.second.m_type.find("C") != std::string::npos;
				if( !hasTexture && !hasColor && !hasVertexColor ) continue;

				if( hasTexture ) {
					vvh::BufferPerObjectTexture uboTexture{};
					uboTexture.model = LtoW(); 		
					uboTexture.modelInverseTranspose = glm::inverse( glm::transpose(uboTexture.model) );
					UVScale uvScale{ { 1.0f, 1.0f }};
					if( m_registry.template Has<UVScale>(oHandle) ) { uvScale = m_registry.template Get<UVScale>(oHandle); }
					uboTexture.uvScale = uvScale;
					memcpy(uniformBuffers().m_uniformBuffersMapped[m_vkState().m_currentFrame], &uboTexture, sizeof(uboTexture));
				} else if( hasColor ) {
					vvh::BufferPerObjectColor uboColor{};
					uboColor.model = LtoW(); 		
					uboColor.modelInverseTranspose = glm::inverse( glm::transpose(uboColor.model) );
					uboColor.color = m_registry.template Get<vvh::Color>(oHandle);
					memcpy(uniformBuffers().m_uniformBuffersMapped[m_vkState().m_currentFrame], &uboColor, sizeof(uboColor));
				} else if( hasVertexColor ) {
					vvh::BufferPerObject uboColor{};
					uboColor.model = LtoW(); 
					uboColor.modelInverseTranspose = glm::inverse( glm::transpose(uboColor.model) );
					memcpy(uniformBuffers().m_uniformBuffersMapped[m_vkState().m_currentFrame], &uboColor, sizeof(uboColor));
				}
			}
		}
		return false;
	}

    bool RendererForward11::OnRecordNextFrame(Message message) {
		auto msg = message.template GetData<MsgRecordNextFrame>();

		std::vector<VkCommandBuffer> cmdBuffers(1);
		vvh::ComCreateCommandBuffers({
			m_vkState().m_device, 
			m_commandPools[m_vkState().m_currentFrame], 
			cmdBuffers
		});
		auto cmdBuffer = cmdBuffers[0];

		vvh::ComBeginCommandBuffer({cmdBuffer});

		vvh::ComBeginRenderPass({
			cmdBuffer, 
			m_vkState().m_imageIndex, 
			m_vkState().m_swapChain, 
			m_renderPass, 
			false, 
			{}, 
			m_vkState().m_currentFrame
		});

		float f = 0.0;
		std::array<float,4> blendconst = (m_pass == 0 ? std::array<float,4>{f,f,f,f} : std::array<float,4>{1-f,1-f,1-f,1-f});
		
		for( auto& pipeline : m_pipelinesPerType) {

			vvh::LightOffset offset{0, m_numberLightsPerType.x + m_numberLightsPerType.y + m_numberLightsPerType.z};
			//vv::LightOffset offset{m_pass, 1};

			vvh::Pipeline pip {
				pipeline.second.m_graphicsPipeline.m_pipelineLayout,
				pipeline.second.m_graphicsPipeline.m_pipeline
			};

			vvh::ComBindPipeline({
				cmdBuffer, 
				pip, 
				m_vkState().m_imageIndex, 
				m_vkState().m_swapChain, 
				m_renderPass, 
				{},	
				{}, 
				blendconst, //blend constants
				{
					{	.layout = pipeline.second.m_graphicsPipeline.m_pipelineLayout, 
						.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, 
						.offset = 0, 
						.size = sizeof(offset), 
						.pValues = &offset
					}
				}, //push constants
				m_vkState().m_currentFrame
			});

			for( auto[oHandle, name, ghandle, LtoW, uniformBuffers, descriptorsets] : 
				m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vvh::Buffer&, vvh::DescriptorSet&>
						({(size_t)pipeline.second.m_graphicsPipeline.m_pipeline}) ) {

				bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
				bool hasColor = m_registry.template Has<vvh::Color>(oHandle);
				bool hasVertexColor = pipeline.second.m_type.find("C") != std::string::npos;
				if( !hasTexture && !hasColor && !hasVertexColor ) continue;

				auto mesh = m_registry.template Get<vvh::Mesh&>(ghandle);
				vvh::ComRecordObject( {
					cmdBuffer, 
					pipeline.second.m_graphicsPipeline, 
					{ m_descriptorSetPerFrame, descriptorsets }, 
					pipeline.second.m_type, 
					mesh, 
					m_vkState().m_currentFrame 
				});
			}
		}

		vvh::ComEndRenderPass({.m_commandBuffer = cmdBuffer});
		vvh::ComEndCommandBuffer({.m_commandBuffer = cmdBuffer});
	    SubmitCommandBuffer(cmdBuffer);

		++m_pass;
		return false;
    }

	bool RendererForward11::OnObjectCreate( Message message ) {
		ObjectHandle oHandle = message.template GetData<MsgObjectCreate>().m_object;
		assert( m_registry.template Has<MeshHandle>(oHandle) );	
		auto meshHandle = m_registry.template Get<MeshHandle>(oHandle);
		auto mesh = m_registry.template Get<vvh::Mesh&>(meshHandle);
		auto type = getPipelineType(oHandle, mesh().m_verticesData);
		auto pipelinePerType = getPipelinePerType(type);

		bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
		bool hasColor = m_registry.template Has<vvh::Color>(oHandle);
		bool hasVertexColor = pipelinePerType->m_type.find("C") != std::string::npos;
		if( !hasTexture && !hasColor && !hasVertexColor ) return false;	

		vvh::Buffer ubo;
		size_t sizeUbo = 0;
		vvh::DescriptorSet descriptorSet{1};
		vvh::RenCreateDescriptorSet({
			m_vkState().m_device, 
			pipelinePerType->m_descriptorSetLayoutPerObject, 
			m_descriptorPool, 
			descriptorSet
		});

		if( hasTexture ) {
			sizeUbo = sizeof(vvh::BufferPerObjectTexture);
			auto tHandle = m_registry.template Get<TextureHandle>(oHandle);
			auto texture = m_registry.template Get<vvh::Image&>(tHandle);
	    	vvh::RenUpdateDescriptorSetTexture({
				m_vkState().m_device, 
				texture, 
				1,
				descriptorSet
			});
		} else if(hasColor) {
			sizeUbo = sizeof(vvh::BufferPerObjectColor);
		} else if(hasVertexColor) {
			sizeUbo = sizeof(vvh::BufferPerObject);
		}

		vvh::BufCreateBuffers({
			m_vkState().m_device, 
			m_vkState().m_vmaAllocator, 
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			sizeUbo, 
			ubo
		});
	    vvh::RenUpdateDescriptorSet({
			m_vkState().m_device, 
			ubo, 
			0, 
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
			sizeUbo, 
			descriptorSet
		});

		m_registry.Put(oHandle, ubo, descriptorSet);
		m_registry.AddTags(oHandle, (size_t)pipelinePerType->m_graphicsPipeline.m_pipeline);

		assert( m_registry.template Has<vvh::Buffer>(oHandle) );
		assert( m_registry.template Has<vvh::DescriptorSet>(oHandle) );
		return false; //true if handled
	}

	bool RendererForward11::OnObjectDestroy( Message message ) {
		auto msg = message.template GetData<MsgObjectDestroy>();
		auto oHandle = msg.m_handle();

		assert(m_registry.Exists(oHandle) );

		if( !m_registry.template Has<vvh::Buffer>(oHandle) ) return false;
		auto ubo = m_registry.template Get<vvh::Buffer&>(oHandle);
		vvh::BufDestroyBuffer2({
			m_vkState().m_device, 
			m_vkState().m_vmaAllocator, 
			ubo
		});
		return false;
	}


	/// @brief 
	/// @param handle 
	/// @param vertexData 
	/// @return 
	std::string RendererForward11::getPipelineType(ObjectHandle handle, vvh::VertexData &vertexData) {
		std::string type = vertexData.getType();
		if( m_registry.template Has<TextureHandle>(handle) && type.find("U") != std::string::npos ) type += "E";
		if( m_registry.template Has<vvh::Color>(handle) && type.find("C") == std::string::npos && type.find("E") == std::string::npos ) type += "O";
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
        vkDeviceWaitIdle(m_vkState().m_device);

		for( auto pool : m_commandPools ) {
			vkDestroyCommandPool(m_vkState().m_device, pool, nullptr);
		}

		for( auto& [type, pipeline] : m_pipelinesPerType ) {
			vkDestroyDescriptorSetLayout(m_vkState().m_device, pipeline.m_descriptorSetLayoutPerObject, nullptr);
			vkDestroyPipeline(m_vkState().m_device, pipeline.m_graphicsPipeline.m_pipeline, nullptr);
			vkDestroyPipelineLayout(m_vkState().m_device, pipeline.m_graphicsPipeline.m_pipelineLayout, nullptr);
		}

        vkDestroyDescriptorPool(m_vkState().m_device, m_descriptorPool, nullptr);		
		vkDestroyRenderPass(m_vkState().m_device, m_renderPass, nullptr);
		vkDestroyRenderPass(m_vkState().m_device, m_renderPassClear, nullptr);
		vvh::BufDestroyBuffer2({m_vkState().m_device, m_vkState().m_vmaAllocator, m_uniformBuffersPerFrame});
		vvh::BufDestroyBuffer2({m_vkState().m_device, m_vkState().m_vmaAllocator, m_storageBuffersLights});
		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerFrame, nullptr);

		return false;
    }


};   // namespace vve


