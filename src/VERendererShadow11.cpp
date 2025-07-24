//#include "VHInclude.h"
#include "VHInclude2.h"
#include "VEInclude.h"



namespace vve {

	RendererShadow11::RendererShadow11(const std::string& systemName, Engine& engine, const std::string& windowName ) : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallbacks( { 
			{this,  3400, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,  1800, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
			{this,  1990, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,  1700, "OBJECT_CREATE",		[this](Message& message) { return OnObjectCreate(message); } },
			{this, 10000, "OBJECT_DESTROY",		[this](Message& message) { return OnObjectDestroy(message); } },
			{this,     0, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );
	};

	RendererShadow11::~RendererShadow11(){};

	bool RendererShadow11::OnInit(const Message& message) {
		Renderer::OnInit(message);

		vvh::RenCreateRenderPassShadow({
			m_vkState().m_depthMapFormat,
			m_vkState().m_device,
			true,
			m_renderPass
			});

		m_commandPools.resize(MAX_FRAMES_IN_FLIGHT);
		m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vvh::ComCreateCommandPool({
				.m_surface = m_vkState().m_surface,
				.m_physicalDevice = m_vkState().m_physicalDevice,
				.m_device = m_vkState().m_device,
				.m_commandPool = m_commandPools[i]
				});

			vvh::ComCreateSingleCommandBuffer({
				.m_device = m_vkState().m_device,
				.m_commandPool = m_commandPools[i],
				.m_commandBuffer = m_commandBuffers[i]
				});
		}


		vvh::RenCreateDescriptorPool({ m_vkState().m_device, 1000, m_descriptorPool });

		vvh::RenCreateDescriptorSetLayout({ m_vkState().m_device,
			{ {.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT } },
			m_descriptorSetLayoutPerObject });

		std::vector<VkVertexInputBindingDescription> bindingDescriptions = getBindingDescriptions("P");
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions = getAttributeDescriptions("P");

		// TODO: Move shadow into own folder so deferred AND forward can use it
		vvh::RenCreateGraphicsPipeline({
			m_vkState().m_device,
			m_renderPass,
			"shaders/Deferred/Shadow11.spv", "shaders/Deferred/Shadow11.spv",
			bindingDescriptions, attributeDescriptions,
			{ m_descriptorSetLayoutPerObject },
			{}, //spezialization constants
			{ {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(PushConstantShadow)} },
			{}, //blend attachments
			m_shadowPipeline,
			{}, //
			m_vkState().m_depthMapFormat,
			true,
			VK_CULL_MODE_FRONT_BIT
		});	

		std::cout << "Pipeline Shadow" << std::endl;

		// Dummy Image 
		vvh::ImgCreateImage({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_width = 1,
			.m_height = 1,
			.m_depth = 1,
			.m_layers = 6,
			.m_mipLevels = 1,
			.m_format = m_vkState().m_depthMapFormat,
			.m_tiling = VK_IMAGE_TILING_OPTIMAL,
			.m_usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.m_imageLayout = VK_IMAGE_LAYOUT_UNDEFINED, 
			.m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			.m_image = m_dummyImage.m_dummyImage,
			.m_imageAllocation = m_dummyImage.m_dummyImageAllocation,
			.m_imgCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
			});

		// Depth image VK_IMAGE_LAYOUT_UNDEFINED --> VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
		vvh::ImgTransitionImageLayout3({
			.m_device = m_vkState().m_device,
			.m_graphicsQueue = m_vkState().m_graphicsQueue,
			.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
			.m_image = m_dummyImage.m_dummyImage,
			.m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
			.m_layers = 6,
			.m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.m_newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
		});

		// Shadow Image
		ShadowImage shadowImage{
			.maxImageDimension2D = std::min(m_vkState().m_physicalDeviceProperties.limits.maxImageDimension2D, SHADOW_MAP_DIMENSION),
			.maxImageArrayLayers = std::min(m_vkState().m_physicalDeviceProperties.limits.maxImageArrayLayers, SHADOW_MAX_NUM_LAYERS)
		};
		std::cout << "MAX IMAGE LAYERS: " << shadowImage.maxImageArrayLayers << "\n";
		m_shadowImageHandle = m_registry.Insert(shadowImage);
		// TODO: Manage tag better
		m_registry.AddTags(m_shadowImageHandle, (size_t)1337);

		SetDummyCubeArrayView();
		SetDummy2DArrayView();

		return false;
	}

	void RendererShadow11::SetDummyCubeArrayView() {
		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);
		shadowImage().m_cubeArrayView = vvh::ImgCreateImageView({
			.m_device = m_vkState().m_device,
			.m_image = m_dummyImage.m_dummyImage,
			.m_format = m_vkState().m_depthMapFormat,
			.m_aspects = VK_IMAGE_ASPECT_DEPTH_BIT,
			.m_layers = 6,
			.m_mipLevels = 1,
			.m_viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
			});
	}

	void RendererShadow11::SetDummy2DArrayView() {
		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);
		shadowImage().m_2DArrayView = vvh::ImgCreateImageView({
			.m_device = m_vkState().m_device,
			.m_image = m_dummyImage.m_dummyImage,
			.m_format = m_vkState().m_depthMapFormat,
			.m_aspects = VK_IMAGE_ASPECT_DEPTH_BIT,
			.m_layers = 6,
			.m_mipLevels = 1,
			.m_viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
			.m_baseArrayLayer = 0
			});
	}


	void RendererShadow11::CreateShadowMap() {	
		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);

		uint32_t numPointShadows = CountShadows<PointLight>(6);
		uint32_t numDirectShadows = CountShadows<DirectionalLight>(1);
		uint32_t numSpotShadows = CountShadows<SpotLight>(1);
		uint32_t numTotalLayers = numPointShadows + numDirectShadows + numSpotShadows;
		numTotalLayers = numTotalLayers < 6 ? 6 : numTotalLayers;	// Because of cube compatible min 6 layers

		// TODO: Currently there is no cap on layers, could exceed allocation size or maxImageArrayLayers
		assert(numTotalLayers < shadowImage().maxImageArrayLayers);

		// TODO: make destroy function
		DestroyShadowMap();

		vvh::Image map;
		vvh::ImgCreateImage({ m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator
			, shadowImage().maxImageDimension2D, shadowImage().maxImageDimension2D, 1, numTotalLayers, 1
			, m_vkState().m_depthMapFormat, VK_IMAGE_TILING_OPTIMAL
			, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
			, VK_IMAGE_LAYOUT_UNDEFINED
			, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, map.m_mapImage, map.m_mapImageAllocation, 
			VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT // Cube now!
			});

		// light space matrices needed only by direct and spot lights
		uint32_t numDirectAndSpotShadows = numDirectShadows + numSpotShadows;
		shadowImage().m_lightSpaceMatrices.clear();
		shadowImage().m_lightSpaceMatrices.reserve(numDirectAndSpotShadows);
		shadowImage().shadowImage = map;
		shadowImage().numberImageArraylayers = numTotalLayers;

		// transition shadowMap from VK_IMAGE_LAYOUT_UNDEFINED --> VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		vvh::ImgTransitionImageLayout3({
			.m_device = m_vkState().m_device,
			.m_graphicsQueue = m_vkState().m_graphicsQueue,
			.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
			.m_image = shadowImage().shadowImage.m_mapImage,
			.m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
			.m_layers = (int)numTotalLayers,
			.m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.m_newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.m_commandBuffer = VK_NULL_HANDLE
		});

		std::cout << "\nNumber of shadow layers: " << numTotalLayers << "\n\n";

		// TODO: shadowImage().shadowImage.m_mapImageView is pointless like that in 1.1!

		// Cube Array View for Point Lights
		if (numPointShadows >= 6) {
			shadowImage().m_cubeArrayView = vvh::ImgCreateImageView({
			.m_device = m_vkState().m_device,
			.m_image = shadowImage().shadowImage.m_mapImage,
			.m_format = m_vkState().m_depthMapFormat,
			.m_aspects = VK_IMAGE_ASPECT_DEPTH_BIT,
			.m_layers = numPointShadows,
			.m_mipLevels = 1,
			.m_viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
			});
		}
		else {
			SetDummyCubeArrayView();
		}
		

		// 2D Array View for Direct + Spot Lights
		if (numDirectAndSpotShadows > 0) {
			shadowImage().m_2DArrayView = vvh::ImgCreateImageView({
				.m_device = m_vkState().m_device,
				.m_image = shadowImage().shadowImage.m_mapImage,
				.m_format = m_vkState().m_depthMapFormat,
				.m_aspects = VK_IMAGE_ASPECT_DEPTH_BIT,
				.m_layers = numDirectAndSpotShadows,
				.m_mipLevels = 1,
				.m_viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
				.m_baseArrayLayer = numPointShadows
				});
		}
		else {
			SetDummy2DArrayView();
		}

		// Creates a view and framebuffer for the total number of shadow layers
		m_layerViews.resize(numTotalLayers);
		m_shadowFrameBuffers.resize(numTotalLayers);
		for (uint32_t i = 0; i < numTotalLayers; ++i) {
			m_layerViews[i] = vvh::ImgCreateImageView({
				.m_device = m_vkState().m_device,
				.m_image = shadowImage().shadowImage.m_mapImage,
				.m_format = m_vkState().m_depthMapFormat,
				.m_aspects = VK_IMAGE_ASPECT_DEPTH_BIT,
				.m_layers = 1,
				.m_mipLevels = 1,
				.m_viewType = VK_IMAGE_VIEW_TYPE_2D,
				.m_baseArrayLayer = i
				});

			// Framebuffer with layers
			vvh::RenCreateSingleFrameBuffer({
				.m_device = m_vkState().m_device,
				.m_renderPass = m_renderPass,
				.m_frameBuffer = m_shadowFrameBuffers[i],
				.m_imageView = m_layerViews[i],
				.m_extent = {shadowImage().maxImageDimension2D, shadowImage().maxImageDimension2D},
				.m_numLayers = 1
			});
		}
	}

	template<typename T>
	uint32_t RendererShadow11::CountShadows(const uint32_t& shadowsPerLight) const {
		int n=0;
		for( auto [handle, light] : m_registry.template GetView<vecs::Handle, T&>() ) {

			++n;
		};
		return n*shadowsPerLight;
	}

	bool RendererShadow11::OnPrepareNextFrame(const Message& message) {
		if (m_state != State::STATE_NEW || !m_engine.IsShadowEnabled()) return false;
		m_state = State::STATE_PREPARED;

		vkResetCommandPool(m_vkState().m_device, m_commandPools[m_vkState().m_currentFrame], 0);
	
		CreateShadowMap();
			
		return false;
	}

	bool RendererShadow11::OnRecordNextFrame(const Message& message) {
		if (m_state != State::STATE_PREPARED || !m_engine.IsShadowEnabled()) return false;
		m_state = State::STATE_RECORDED;

		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);

		auto& cmdBuffer = m_commandBuffers[m_vkState().m_currentFrame];
		vvh::ComBeginCommandBuffer({ cmdBuffer });

		// TODO: has swapchain extent
		vvh::ComBindPipeline({
			.m_commandBuffer = cmdBuffer,
			.m_graphicsPipeline = m_shadowPipeline,
			.m_extent = {shadowImage().maxImageDimension2D, shadowImage().maxImageDimension2D},
			.m_viewPorts = {},
			.m_scissors = {}, //default view ports and scissors
			.m_blendConstants = {},
			.m_pushConstants = {}
			});

		// Takes the object ubo from OnObjectCreate of a renderer (not the shadow renderer) to update the descriptor set
		for (auto [oHandle, descriptorset] : m_registry.template GetView<vecs::Handle, oShadowDescriptor&> ({ (size_t)m_shadowPipeline.m_pipeline })) {
			assert(m_registry.template Has<vvh::Buffer>(oHandle));
			vvh::Buffer& ubo = m_registry.template Get<vvh::Buffer&>(oHandle);
			size_t sizeUbo = sizeof(vvh::BufferPerObject);	// Always BufferPerObject as only position is used, ignore rest
			vvh::RenUpdateDescriptorSet({
				.m_device = m_vkState().m_device,
				.m_uniformBuffers = ubo,
				.m_binding = 0,
				.m_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.m_size = sizeUbo,
				.m_descriptorSet = descriptorset().m_oShadowDescriptor
				});
		}

		uint32_t numberTotalLayers = shadowImage().numberImageArraylayers;
		uint32_t layerIdx = 0;
		static constexpr float near = 0.1f;
		static constexpr float far = 1000.0f;

		RenderPointLightShadow(cmdBuffer, layerIdx, near, far);
		RenderDirectLightShadow(cmdBuffer, layerIdx, near, far);
		RenderSpotLightShadow(cmdBuffer, layerIdx, near, far);

		std::cout << "Shadow lsm matrices count: " << shadowImage().m_lightSpaceMatrices.size() << std::endl;

		// Depth image VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL --> VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
		vvh::ImgTransitionImageLayout3({
			.m_device = m_vkState().m_device,
			.m_graphicsQueue = m_vkState().m_graphicsQueue,
			.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
			.m_image = shadowImage().shadowImage.m_mapImage,
			.m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
			.m_layers = (int)numberTotalLayers,	// TODO: fix when it works
			.m_oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.m_newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			.m_commandBuffer = cmdBuffer,
		});

		vvh::ComEndCommandBuffer({ .m_commandBuffer = cmdBuffer });
		SubmitCommandBuffer(cmdBuffer);

		m_engine.SendMsg(MsgShadowMapRecreated{});
		return false;
	}

	bool RendererShadow11::OnObjectCreate(Message& message) {
		const ObjectHandle& oHandle = message.template GetData<MsgObjectCreate>().m_object;
		if (m_registry.template Has<DirectionalLight>(oHandle)) return false;	// Object without mesh, e.g. direct light

		assert(m_registry.template Has<MeshHandle>(oHandle));

		vvh::DescriptorSet descriptorSet{ 0 };
		vvh::RenCreateDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_descriptorSetLayouts = m_descriptorSetLayoutPerObject,
			.m_descriptorPool = m_descriptorPool,
			.m_descriptorSet = descriptorSet
			});

		m_registry.AddTags(oHandle, (size_t)m_shadowPipeline.m_pipeline);
		oShadowDescriptor ds = { descriptorSet };
		m_registry.Put(oHandle, ds);

		if (m_state != State::STATE_PREPARED) m_state = State::STATE_NEW;
		return false;
	}

	bool RendererShadow11::OnObjectDestroy(Message& message) {
		const auto& msg = message.template GetData<MsgObjectDestroy>();
		const auto& oHandle = msg.m_handle();
		if (m_registry.template Has<oShadowDescriptor&>(oHandle)) {
			oShadowDescriptor& vvh_ds = m_registry.template Get<oShadowDescriptor&>(oHandle);
			for (auto& ds : vvh_ds.m_oShadowDescriptor.m_descriptorSetPerFrameInFlight) {
				vkFreeDescriptorSets(m_vkState().m_device, m_descriptorPool, 1, &ds);
			}
		}

		if (m_state != State::STATE_PREPARED) m_state = State::STATE_NEW;
		return false;
	}

	bool RendererShadow11::OnQuit(const Message& message) {
        vkDeviceWaitIdle(m_vkState().m_device);

		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);

		for (auto pool : m_commandPools) {
			vkDestroyCommandPool(m_vkState().m_device, pool, nullptr);
		}

        vkDestroyDescriptorPool(m_vkState().m_device, m_descriptorPool, nullptr);
		vkDestroyRenderPass(m_vkState().m_device, m_renderPass, nullptr);
		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerObject, nullptr);

		vkDestroyPipeline(m_vkState().m_device, m_shadowPipeline.m_pipeline, nullptr);
		vkDestroyPipelineLayout(m_vkState().m_device, m_shadowPipeline.m_pipelineLayout, nullptr);

		vvh::ImgDestroyImage({ m_vkState().m_device, m_vkState().m_vmaAllocator,
				m_dummyImage.m_dummyImage, m_dummyImage.m_dummyImageAllocation });
		DestroyShadowMap();
		
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
		vkDestroyImageView(m_vkState().m_device, shadowImage().m_cubeArrayView, nullptr);
		vkDestroyImageView(m_vkState().m_device, shadowImage().m_2DArrayView, nullptr);
	}

	void RendererShadow11::RenderPointLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near, const float& far) {
		const ShadowImage& shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);
		float aspect = 1.0f;	// width / height

		static glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), aspect, near, far);

		for (auto [handle, light, lToW] : m_registry.template GetView<vecs::Handle, PointLight&, LocalToWorldMatrix&>()) {
			glm::vec3 lightPos = glm::vec3{ lToW()[3] };

			static std::vector<glm::mat4> shadowTransforms;
			shadowTransforms.clear();
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


			for (uint8_t i = 0; i < 6; ++i) {
				glm::mat4& lightSpaceMatrix = shadowTransforms[i];
				PushConstantShadow pc{ lightSpaceMatrix, lightPos };

				vkCmdPushConstants(cmdBuffer,
					m_shadowPipeline.m_pipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0,
					sizeof(PushConstantShadow),
					&pc
				);

				RenderShadowMap(cmdBuffer, layer);
			}
		}
	}

	void RendererShadow11::RenderDirectLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near, const float& far) {
		ShadowImage& shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);
		static constexpr glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
		glm::vec3 sceneCenter(0.0f);

		// TODO: left right bottom top is set to fit for deferred-demo.cpp, make adjustable?
		glm::mat4 shadowProj = glm::ortho(-20.0f, +20.0f, -20.0f, +20.0f, near, far);

		for (auto [handle, light, lToW] : m_registry.template GetView<vecs::Handle, DirectionalLight&, LocalToWorldMatrix&>()) {
			glm::vec3 lightDir = glm::normalize(glm::vec3{ lToW()[1] });
			glm::vec3 lightPos = sceneCenter - lightDir * 20.0f;

			std::cout << "DirectLightPos: " << lightPos.x << ", " << lightPos.y << ", " << lightPos.z << std::endl;
			std::cout << "DirectLightDir: " << lightDir.x << ", " << lightDir.y << ", " << lightDir.z << std::endl;

			glm::mat4 view = glm::lookAt(lightPos, sceneCenter, up);
			glm::mat4 lightSpaceMatrix = shadowProj * view;
			// Push for lsm ubo in renderer
			shadowImage.m_lightSpaceMatrices.emplace_back(lightSpaceMatrix);

			PushConstantShadow pc{ lightSpaceMatrix, lightPos };
			vkCmdPushConstants(cmdBuffer,
				m_shadowPipeline.m_pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(PushConstantShadow),
				&pc
			);

			RenderShadowMap(cmdBuffer, layer);
		}
	}

	void RendererShadow11::RenderSpotLightShadow(const VkCommandBuffer& cmdBuffer, uint32_t& layer, const float& near, const float& far) {
		ShadowImage& shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);
		float aspect = 1.0f;	// width / height
		static constexpr glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);

		glm::mat4 shadowProj = glm::perspective(glm::radians(120.0f), aspect, near, far);

		for (auto [handle, light, lToW] : m_registry.template GetView<vecs::Handle, SpotLight&, LocalToWorldMatrix&>()) {
			glm::vec3 lightPos = glm::vec3{ lToW()[3] };
			glm::vec3 lightDir = glm::vec3{ lToW()[1] };

			std::cout << "SpotLightDir: " << lightDir.x << ", " << lightDir.y << ", " << lightDir.z << std::endl;

			glm::mat4 view = glm::lookAt(lightPos, lightPos + lightDir, up );
			glm::mat4 lightSpaceMatrix = shadowProj * view;
			// Push for lsm ubo in renderer
			shadowImage.m_lightSpaceMatrices.emplace_back(lightSpaceMatrix);

			PushConstantShadow pc{ lightSpaceMatrix, lightPos };
			vkCmdPushConstants(cmdBuffer,
				m_shadowPipeline.m_pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(PushConstantShadow),
				&pc
			);

			RenderShadowMap(cmdBuffer, layer);
		}
	}

	void RendererShadow11::RenderShadowMap(const VkCommandBuffer& cmdBuffer, uint32_t& layer) {
		const ShadowImage& shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);

		// One renderPass per layer, image index = layerNumber
		vvh::ComBeginRenderPass2({
			.m_commandBuffer = cmdBuffer,
			.m_imageIndex = layer++,	//m_vkState().m_imageIndex,
			.m_extent = {shadowImage.maxImageDimension2D, shadowImage.maxImageDimension2D},
			.m_framebuffers = m_shadowFrameBuffers,
			.m_renderPass = m_renderPass,
			.m_clearValues = m_clearValue
		});

		for (auto [oHandle, ghandle, descriptorset] :
			m_registry.template GetView<vecs::Handle, MeshHandle, oShadowDescriptor&>
			({ (size_t)m_shadowPipeline.m_pipeline })) {

			if (m_registry.template Has<PointLight>(oHandle) || m_registry.template Has<SpotLight>(oHandle)) {
				// Renders depth image without the point or spot light sphere
				continue;
			}

			const vvh::Mesh& mesh = m_registry.template Get<vvh::Mesh&>(ghandle);

			vvh::ComRecordObject({
				.m_commandBuffer = cmdBuffer,
				.m_graphicsPipeline = m_shadowPipeline,
				.m_descriptorSets = { descriptorset().m_oShadowDescriptor },
				.m_type = "P",
				.m_mesh = mesh,
				.m_currentFrame = m_vkState().m_currentFrame
				});
		}

		vvh::ComEndRenderPass({ .m_commandBuffer = cmdBuffer });
	}

};   // namespace vve