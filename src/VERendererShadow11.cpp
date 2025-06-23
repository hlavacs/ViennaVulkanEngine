//#include "VHInclude.h"
#include "VHInclude2.h"
#include "VEInclude.h"



namespace vve {

	RendererShadow11::RendererShadow11( std::string systemName, Engine& engine, std::string windowName ) : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallbacks( { 
			{this,  3500, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,  2100, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
			{this,  1990, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,  3250, "OBJECT_CREATE",		[this](Message& message) { return OnObjectCreate(message); } },
			{this, 10000, "OBJECT_DESTROY",		[this](Message& message) { return OnObjectDestroy(message); } },
			{this,     0, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );
	};

	RendererShadow11::~RendererShadow11(){};

	bool RendererShadow11::OnInit(Message message) {
		Renderer::OnInit(message);

		vvh::RenCreateRenderPassShadow({
			m_vkState().m_depthMapFormat,
			m_vkState().m_device,
			true,
			m_renderPass
			});

		vvh::ComCreateCommandPool({ m_vkState().m_surface, m_vkState().m_physicalDevice, m_vkState().m_device, m_commandPool });
		vvh::ComCreateCommandBuffers({ m_vkState().m_device, m_commandPool, m_commandBuffers });

		// -----------------------------------------------------------------------------------------------

		vvh::RenCreateDescriptorSetLayout({ m_vkState().m_device, //Per frame buffers
			{
				{.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
				},
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
				}
			},
			m_descriptorSetLayoutPerFrame });
		vvh::RenCreateDescriptorPool({ m_vkState().m_device, 1000, m_descriptorPool });
		vvh::RenCreateDescriptorSet({ m_vkState().m_device, m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetPerFrame });

		// -----------------------------------------------------------------------------------------------

		//VkDescriptorSetLayout descriptorSetLayoutPerObject = VK_NULL_HANDLE;
		std::vector<VkDescriptorSetLayoutBinding> bindings{
			{.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT }
		};
		vvh::RenCreateDescriptorSetLayout({ m_vkState().m_device, bindings, m_descriptorSetLayoutPerObject });

		std::vector<VkVertexInputBindingDescription> bindingDescriptions = getBindingDescriptions("P");
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions = getAttributeDescriptions("P");

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};

		// TODO: Move shadow into own folder so deferred AND forward can use it
		vvh::RenCreateGraphicsPipeline({
			m_vkState().m_device,
			m_renderPass,
			"shaders/Deferred/Shadow11.spv", "",
			bindingDescriptions, attributeDescriptions,
			{ m_descriptorSetLayoutPerFrame, m_descriptorSetLayoutPerObject },
			{}, //spezialization constants
			{ {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(glm::mat4)} }, //push constant ranges -> 2 ints
			{}, //blend attachments
			m_shadowPipeline,
			{}, //
			vvh::RenFindDepthFormat(m_vkState().m_physicalDevice),
			true
		});
		

		std::cout << "Pipeline Shadow" << std::endl;


		// -----------------------------------------------------------------------------------------------
		
		//Per frame uniform buffer
		vvh::BufCreateBuffers({ m_vkState().m_device, m_vkState().m_vmaAllocator,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(vvh::UniformBufferFrame), m_uniformBuffersPerFrame });
		vvh::RenUpdateDescriptorSet({ m_vkState().m_device, m_uniformBuffersPerFrame, 0,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeof(vvh::UniformBufferFrame), m_descriptorSetPerFrame });
	
		//Per frame lights buffer
		vvh::BufCreateBuffers({ m_vkState().m_device, m_vkState().m_vmaAllocator,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, MAX_NUMBER_LIGHTS * sizeof(vvh::ShadowIndex) * 6, m_storageBuffersLights });
		vvh::RenUpdateDescriptorSet({ m_vkState().m_device, m_storageBuffersLights, 1,
			VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_NUMBER_LIGHTS * sizeof(vvh::ShadowIndex) * 6, m_descriptorSetPerFrame });

		// -----------------------------------------------------------------------------------------------

		ShadowImage shadowImage {
			.maxImageDimension2D = std::min(m_vkState().m_physicalDeviceProperties.limits.maxImageDimension2D, SHADOW_MAX_MAPS_PER_ROW*SHADOW_MAP_DIMENSION),
			.maxImageArrayLayers = std::min(m_vkState().m_physicalDeviceProperties.limits.maxImageArrayLayers, SHADOW_MAX_NUM_LAYERS)
		};
		if( m_vkState().m_physicalDeviceProperties.limits.maxImageArrayLayers >= MAX_NUMBER_LIGHTS*6) {
			shadowImage.maxImageDimension2D = SHADOW_MAP_DIMENSION;
			shadowImage.maxImageArrayLayers = MAX_NUMBER_LIGHTS*6;
		}
		m_shadowImageHandle = m_registry.Insert(shadowImage);
		m_registry.AddTags(m_shadowImageHandle, (size_t)1337 );

		return false;
	}


	void RendererShadow11::CheckShadowMaps( uint32_t numberMapsRequired ) {
		
		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);
		auto numberMaps = std::min(numberMapsRequired, shadowImage().MaxNumberMapsPerImage());
		if( shadowImage().NumberMapsPerImage() >= numberMaps ) return;

		uint32_t requiredLayers = (uint32_t)std::ceil(numberMaps / (float)shadowImage().MaxNumberMapsPerLayer());
		auto numLayers = std::min(shadowImage().maxImageArrayLayers, requiredLayers);
		vvh::ImgDestroyImage({ m_vkState().m_device, m_vkState().m_vmaAllocator,
				shadowImage().shadowImage.m_mapImage, shadowImage().shadowImage.m_mapImageAllocation });

		vvh::Image map;
		vvh::ImgCreateImage({ m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator
			, shadowImage().maxImageDimension2D, shadowImage().maxImageDimension2D, 1, numLayers, 1
			, vvh::RenFindDepthFormat(m_vkState().m_physicalDevice), VK_IMAGE_TILING_OPTIMAL
			, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
			, VK_IMAGE_LAYOUT_UNDEFINED
			, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, map.m_mapImage, map.m_mapImageAllocation, 
			VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT // Cube now!
			});

		shadowImage().shadowImage = map;
		shadowImage().numberImageArraylayers = numLayers;

		// transition shadowMap from VK_IMAGE_LAYOUT_UNDEFINED --> VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		vvh::ImgTransitionImageLayout3({
				.m_device = m_vkState().m_device,
				.m_graphicsQueue = m_vkState().m_graphicsQueue,
				.m_commandPool = m_commandPool,
				.m_image = shadowImage().shadowImage.m_mapImage,
				.m_format = vvh::RenFindDepthFormat(m_vkState().m_physicalDevice),
				.m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
				.m_layers = 6,	// TODO: fix when it works
				.m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.m_newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.m_commandBuffer = VK_NULL_HANDLE
			});

		std::cout << "\nNumber of shadow layers: " << numLayers << "\n\n";

		// TODO: shadowImage().shadowImage.m_mapImageView is pointless like that in 1.1!
		shadowImage().m_cubeArrayView = vvh::ImgCreateImageView({
				.m_device = m_vkState().m_device,
				.m_image = shadowImage().shadowImage.m_mapImage,
				.m_format = vvh::RenFindDepthFormat(m_vkState().m_physicalDevice),
				.m_aspects = VK_IMAGE_ASPECT_DEPTH_BIT,
				.m_layers = CountShadows<PointLight>(6),
				.m_mipLevels = 1,
				.m_viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
			});

		m_layerViews.resize(numLayers);
		m_shadowFrameBuffers.resize(numLayers);
		for (uint32_t i = 0; i < numLayers; ++i) {
			m_layerViews[i] = vvh::ImgCreateImageView({
				.m_device = m_vkState().m_device,
				.m_image = shadowImage().shadowImage.m_mapImage,
				.m_format = vvh::RenFindDepthFormat(m_vkState().m_physicalDevice),
				.m_aspects = VK_IMAGE_ASPECT_DEPTH_BIT,
				.m_layers = 1,
				.m_mipLevels = 1,
				.m_viewType = VK_IMAGE_VIEW_TYPE_2D
				});

			// TODO: Framebuffer setup, has to be teared down when stuff changes?
			// Framebuffer with layers
			vvh::RenCreateSingleFrameBuffer({
					.m_device = m_vkState().m_device,
					.m_renderPass = m_renderPass,
					.m_frameBuffer = m_shadowFrameBuffers[i],
					.m_imageView = m_layerViews[i],
					.m_extent = {shadowImage().maxImageDimension2D, shadowImage().maxImageDimension2D},
					.m_numLayers = 1 // shadowImage().numberImageArraylayers for 1.3 with only one view
				});
		}
		// 1.3 2D array view, might add later
		//// create image views and framebuffers
		//shadowImage().shadowImage.m_mapImageView = vvh::ImgCreateImageView({
		//		.m_device = m_vkState().m_device,
		//		.m_image = shadowImage().shadowImage.m_mapImage,
		//		.m_format = vvh::RenFindDepthFormat(m_vkState().m_physicalDevice),
		//		.m_aspects = VK_IMAGE_ASPECT_DEPTH_BIT,
		//		.m_layers = numLayers,
		//		.m_mipLevels = 1,
		//		.m_viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY
		//	});

	}

	template<typename T>
	uint32_t RendererShadow11::CountShadows(uint32_t shadowsPerLight) {
		int n=0;
		for( auto [handle, light] : m_registry.template GetView<vecs::Handle, T&>() ) {

			++n;
		};
		return n*shadowsPerLight;
	}

	/// @brief Prepare the next frame for shadow map rendering.
	/// Calculate number of lights and number of shadow maps. Calculate ShadowIndex values.
	/// Determine the number of passes and register record callback as many times. Create/adapt ShadowImage if necessaary.
	/// @param message 
	/// @return Returns false.
	bool RendererShadow11::OnPrepareNextFrame(Message message) {
		if (m_renderedAlready) return false;
		vkResetCommandPool(m_vkState().m_device, m_commandPool, 0);
		
		//auto msg = message.template GetData<MsgPrepareNextFrame>();
		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);

		m_pass = 0;
		uint32_t numShadows = CountShadows<PointLight>(6);
		numShadows += CountShadows<DirectionalLight>(1);	// TODO: Was 3 -> that has to be wrong?
		numShadows += CountShadows<SpotLight>(1);
		// TODO: rewrite so it's more understandable
		// Calculates number of layers, creates a vvh::Image shadowMap, creates 
		// a view per layer, creates a frame buffer per layer
		CheckShadowMaps(numShadows);


		//uint32_t numPasses = (uint32_t)std::ceil( numShadows / (float)shadowImage().MaxNumberMapsPerImage());
		//std::cout << "\n\nNumber of Shadow passes: " << numPasses << "\n\n";
		//if( m_numberPasses != numPasses ) {
		//	m_numberPasses = numPasses;
		//	m_engine.DeregisterCallbacks(this, "RECORD_NEXT_FRAME");
		//	for( uint32_t i=0; i<numShadows; ++i) {
		//		m_engine.RegisterCallbacks( { 
		//			{this,  1500 + (int)i*1000, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} }
		//		} );
		//	}
		//}

		vvh::UniformBufferFrame ubc;
		vkResetCommandPool(m_vkState().m_device, m_commandPool, 0);

		static std::vector<vvh::Light> lights{ MAX_NUMBER_LIGHTS };
		int total = 0;

		m_numberLightsPerType = glm::ivec3{ 0 };
		m_numberLightsPerType.x = RegisterLight<PointLight>(1.0f, lights, total);
		m_numberLightsPerType.y = RegisterLight<DirectionalLight>(2.0f, lights, total);
		m_numberLightsPerType.z = RegisterLight<SpotLight>(3.0f, lights, total);

		for (size_t i = 0; i < m_storageBuffersLights.m_uniformBuffersMapped.size(); ++i) {
			memcpy(m_storageBuffersLights.m_uniformBuffersMapped[i], lights.data(), total * sizeof(vvh::Light));
		}

		ubc.numLights = m_numberLightsPerType;

		auto [lToW, view, proj] = *m_registry.template GetView<LocalToWorldMatrix&, ViewMatrix&, ProjectionMatrix&>().begin();
		ubc.camera.view = view();
		ubc.camera.proj = proj();
		ubc.camera.positionW = lToW()[3];
		memcpy(m_uniformBuffersPerFrame.m_uniformBuffersMapped[m_vkState().m_currentFrame], &ubc, sizeof(ubc));

		// per object ubo is already updated by other renderer
			
		return false;
	}

	bool RendererShadow11::OnRecordNextFrame(Message message) {
		if (m_renderedAlready) return false;
		//auto msg = message.template GetData<MsgRecordNextFrame>();
		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);
		++m_pass;

		auto& cmdBuffer = m_commandBuffers[m_vkState().m_currentFrame];
		vvh::ComBeginCommandBuffer({ cmdBuffer });

		// TODO: has swapchain extent
		vvh::ComBindPipeline({
			.m_commandBuffer = cmdBuffer,
			.m_graphicsPipeline = m_shadowPipeline,
			.m_imageIndex = m_vkState().m_imageIndex,
			.m_extent = {shadowImage().maxImageDimension2D, shadowImage().maxImageDimension2D},
			.m_renderPass = m_renderPass,
			.m_viewPorts = {},
			.m_scissors = {}, //default view ports and scissors
			.m_blendConstants = {},
			.m_pushConstants = {},
			.m_currentFrame = m_vkState().m_currentFrame
			});


		//vvh::ShadowOffset shadOff{ .shadowIndexOffset = 0, .numberShadows = 0 };

		uint32_t numberTotalLayers = shadowImage().numberImageArraylayers;
		uint32_t layerIdx = 0;
		float near = 1.0f;
		float far = 15.0f;
		RenderPointLightShadow(cmdBuffer, layerIdx, near, far);
		// TODO: Remove assert. This is temporary, as demo.cpp has 1 point and 1 spot light = 7 layers EXACTLY
		assert(layerIdx == 6);

		// Depth image VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL --> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		vvh::ImgTransitionImageLayout3({
				.m_device = m_vkState().m_device,
				.m_graphicsQueue = m_vkState().m_graphicsQueue,
				.m_commandPool = m_commandPool,
				.m_image = shadowImage().shadowImage.m_mapImage,
				.m_format = m_vkState().m_depthMapFormat,
				.m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
				.m_layers = 6,	// TODO: fix when it works
				.m_oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.m_newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.m_commandBuffer = cmdBuffer,
			});

		vvh::ComEndCommandBuffer({ .m_commandBuffer = cmdBuffer });
		SubmitCommandBuffer(cmdBuffer);

		// TODO: write logic for when shadow is calculated (start, and then only when light changes)
		// TODO: Write own Message maybe?
		//m_engine.DeregisterCallbacks(this, "PREPARE_NEXT_FRAME");
		//m_engine.DeregisterCallbacks(this, "RECORD_NEXT_FRAME");

		m_renderedAlready = true;

		return false;
	}

	bool RendererShadow11::OnObjectCreate(Message message) {
		const ObjectHandle& oHandle = message.template GetData<MsgObjectCreate>().m_object;
		assert(m_registry.template Has<MeshHandle>(oHandle));
		assert(m_registry.template Has<vvh::Buffer>(oHandle));

		vvh::Buffer ubo = m_registry.template Get<vvh::Buffer>(oHandle);
		size_t sizeUbo = sizeof(ubo);
		vvh::DescriptorSet descriptorSet{ 1 };
		vvh::RenCreateDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_descriptorSetLayouts = m_descriptorSetLayoutPerObject,
			.m_descriptorPool = m_descriptorPool,
			.m_descriptorSet = descriptorSet
			});

		vvh::RenUpdateDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_uniformBuffers = ubo,
			.m_binding = 0,
			.m_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.m_size = sizeUbo,
			.m_descriptorSet = descriptorSet
			});

		m_registry.AddTags(oHandle, (size_t)m_shadowPipeline.m_pipeline);
		oShadowDescriptor ds = { descriptorSet };
		m_registry.Put(oHandle, ds);

		return false;
	}

	bool RendererShadow11::OnObjectDestroy(Message message) {
		return false;
	}

	bool RendererShadow11::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vkState().m_device);

        vkDestroyCommandPool(m_vkState().m_device, m_commandPool, nullptr);

        vkDestroyDescriptorPool(m_vkState().m_device, m_descriptorPool, nullptr);
		vkDestroyRenderPass(m_vkState().m_device, m_renderPass, nullptr);
		vvh::BufDestroyBuffer2({ m_vkState().m_device, m_vkState().m_vmaAllocator, m_storageBuffersLights });
		vvh::BufDestroyBuffer2({ m_vkState().m_device, m_vkState().m_vmaAllocator, m_uniformBuffersPerFrame });
		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerFrame, nullptr);
		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerObject, nullptr);

		
		return false;
    }

	void RendererShadow11::DestroyShadowMap() {
		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);

		vvh::ImgDestroyImage({ m_vkState().m_device, m_vkState().m_vmaAllocator,
			shadowImage().shadowImage.m_mapImage, shadowImage().shadowImage.m_mapImageAllocation });

		for (auto& view : m_layerViews) {
			vkDestroyImageView(m_vkState().m_device, view, nullptr);
		}
		for (auto& fb : m_shadowFrameBuffers) {
			vkDestroyFramebuffer(m_vkState().m_device, fb, nullptr);
		}
	}

	void RendererShadow11::RenderPointLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near, const float& far) {
		const ShadowImage& shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);
		float aspect = 1.0f;	// width / height

		// Remapping from OpenGL -1, 1 to Vulkan [0, 1]
		// TODO: What about #define GLM_FORCE_DEPTH_ZERO_TO_ONE
		//glm::mat4 bias = glm::mat4(
		//	1, 0, 0, 0,
		//	0, 1, 0, 0,
		//	0, 0, 0.5, 0.5,
		//	0, 0, 0, 1
		//);

		//const glm::mat4 shadowProj = bias * glm::perspective(glm::radians(90.0f), aspect, near, far);
		const glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), aspect, near, far);


		for (auto [handle, light, lToW] : m_registry.template GetView<vecs::Handle, PointLight&, LocalToWorldMatrix&>()) {
			glm::vec3 lightPos = glm::vec3{ lToW()[3] };

			std::vector<glm::mat4> shadowTransforms;
			shadowTransforms.reserve(6);
			shadowTransforms.push_back(shadowProj *
				glm::lookAt(lightPos, lightPos + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
			shadowTransforms.push_back(shadowProj *
				glm::lookAt(lightPos, lightPos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)));
			shadowTransforms.push_back(shadowProj *
				glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)));
			shadowTransforms.push_back(shadowProj *
				glm::lookAt(lightPos, lightPos + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)));
			shadowTransforms.push_back(shadowProj *
				glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)));
			shadowTransforms.push_back(shadowProj *
				glm::lookAt(lightPos, lightPos + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0)));


			for (uint8_t i = 0; i < 6; ++i, ++layer) {
				// LightSpaceMatrix
				glm::mat4 lightSpaceMatrix = shadowTransforms[i];

				// TODO: image index or just single image?
				// One renderPass per layer, image index = layerNumber
				vvh::ComBeginRenderPass2({
					.m_commandBuffer = cmdBuffer,
					.m_imageIndex = layer,	//m_vkState().m_imageIndex,
					.m_extent = {shadowImage.maxImageDimension2D, shadowImage.maxImageDimension2D},
					.m_framebuffers = m_shadowFrameBuffers,
					.m_renderPass = m_renderPass,
					.m_clearValues = {{.depthStencil = {1.0f, 0} }},
					.m_currentFrame = m_vkState().m_currentFrame
					});


				vkCmdPushConstants(cmdBuffer,
					m_shadowPipeline.m_pipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT,
					0,
					sizeof(glm::mat4),
					&lightSpaceMatrix
				);

				for (auto [oHandle, name, ghandle, LtoW, uniformBuffers, descriptorset] :
					m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vvh::Buffer&, oShadowDescriptor&>
					({ (size_t)m_shadowPipeline.m_pipeline })) {

					if (m_registry.template Has<PointLight>(oHandle)) {
						// Renders depth image without the point light sphere
						continue;
					}

					const vvh::Mesh& mesh = m_registry.template Get<vvh::Mesh&>(ghandle);

					vvh::ComRecordObject({
						.m_commandBuffer = cmdBuffer,
						.m_graphicsPipeline = m_shadowPipeline,
						.m_descriptorSets = {  m_descriptorSetPerFrame, descriptorset().m_oShadowDescriptor},
						.m_type = "P",
						.m_mesh = mesh,
						.m_currentFrame = m_vkState().m_currentFrame
						});
				}

				vvh::ComEndRenderPass({ .m_commandBuffer = cmdBuffer });
			}
		}

	}


};   // namespace vve