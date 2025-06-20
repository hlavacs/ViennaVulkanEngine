//#include "VHInclude.h"
#include "VHInclude2.h"
#include "VEInclude.h"



namespace vve {

	RendererShadow11::RendererShadow11( std::string systemName, Engine& engine, std::string windowName ) : Renderer(systemName, engine, windowName ) {

		engine.RegisterCallbacks( { 
			{this,  3500, "INIT", [this](Message& message){ return OnInit(message);} },
			{this,  1500, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
			{this,  1500, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,     0, "QUIT", [this](Message& message){ return OnQuit(message);} }
		} );
	};

	RendererShadow11::~RendererShadow11(){};

	bool RendererShadow11::OnInit(Message message) {
		Renderer::OnInit(message);

		vvh::RenCreateRenderPassShadow({
			m_vkState().m_depthMapFormat,
			m_vkState().m_device,
			false,
			m_renderPass
			});

		vvh::ComCreateCommandPool({ m_vkState().m_surface, m_vkState().m_physicalDevice, m_vkState().m_device, m_commandPool });
		vvh::ComCreateCommandBuffers({ m_vkState().m_device, m_commandPool, m_commandBuffers });

		// -----------------------------------------------------------------------------------------------

		vvh::RenCreateDescriptorSetLayout({ m_vkState().m_device, //Per frame buffers
			{
				{.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
				},
				{.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
				}
			},
			m_descriptorSetLayoutPerFrame });
		vvh::RenCreateDescriptorPool({ m_vkState().m_device, 1000, m_descriptorPool });
		vvh::RenCreateDescriptorSet({ m_vkState().m_device, m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetPerFrame });

		// -----------------------------------------------------------------------------------------------

		VkDescriptorSetLayout descriptorSetLayoutPerObject;
		std::vector<VkDescriptorSetLayoutBinding> bindings{
			{.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
		};
		vvh::RenCreateDescriptorSetLayout({ m_vkState().m_device, bindings, descriptorSetLayoutPerObject });

		std::vector<VkVertexInputBindingDescription> bindingDescriptions = getBindingDescriptions("P");
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions = getAttributeDescriptions("P");

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};

		// TODO: Move shadow into own folder so deferred AND forward can use it
		vvh::RenCreateGraphicsPipeline({
			m_vkState().m_device,
			m_renderPass,
			"shaders/Deferred/Shadow11.spv", "shaders/Deferred/Shadow11.spv",
			bindingDescriptions, attributeDescriptions,
			{ m_descriptorSetLayoutPerFrame, descriptorSetLayoutPerObject },
			{}, //spezialization constants
			{ {.stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 0, .size = sizeof(vvh::ShadowOffset)} }, //push constant ranges -> 2 ints
			{}, //blend attachments
			m_shadowPipeline
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
			, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, map.m_mapImage, map.m_mapImageAllocation });

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
				.m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.m_newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.m_commandBuffer = VK_NULL_HANDLE
			});

		std::cout << "\nNumber of shadow layers: " << numLayers << "\n\n";

		// TODO: shadowImage().shadowImage.m_mapImageView is pointless like that in 1.1!
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
			//m_shadowFrameBuffers.res
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
		
		//auto msg = message.template GetData<MsgPrepareNextFrame>();
		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);

		m_pass = 0;
		uint32_t numShadows = CountShadows<PointLight>(6);
		numShadows += CountShadows<DirectionalLight>(3);
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

		vvh::ShadowIndex shadowIdx{
			.mapResolution = {SHADOW_MAP_DIMENSION, SHADOW_MAP_DIMENSION},	// glm::ivec2
			.layerIndex = 0,						// uint32_t
			.viewportIndex = 0,						// uint32_t
			.layerOffset = {0, 0},					// glm::ivec2
			.lightSpaceMatrix = glm::mat4(0.0f),	// glm::mat4
		};

		std::vector<vvh::ShadowIndex> shadowStorage;
			
		return false;
	}

	bool RendererShadow11::OnRecordNextFrame(Message message) {
		auto msg = message.template GetData<MsgRecordNextFrame>();
		auto shadowImage = m_registry.template Get<ShadowImage&>(m_shadowImageHandle);
		++m_pass;

		auto& cmdBuffer = m_commandBuffers[m_vkState().m_currentFrame];
		vvh::ComBeginCommandBuffer({ cmdBuffer });

		// TODO: image index or just single image?
		vvh::ComBeginRenderPass2({
			.m_commandBuffer = cmdBuffer,
			.m_imageIndex = 0,	//m_vkState().m_imageIndex,
			.m_extent = {shadowImage().maxImageDimension2D, shadowImage().maxImageDimension2D},
			.m_framebuffers = m_shadowFrameBuffers,
			.m_renderPass = m_renderPass,
			.m_clearValues = {{.depthStencil = {1.0f, 0} }},
			.m_currentFrame = m_vkState().m_currentFrame
			});

		vvh::ComBindPipeline({
			.m_commandBuffer = cmdBuffer,
			.m_graphicsPipeline = m_shadowPipeline,
			.m_imageIndex = m_vkState().m_imageIndex,
			.m_swapChain = m_vkState().m_swapChain,
			.m_renderPass = m_renderPass,
			.m_viewPorts = {},
			.m_scissors = {}, //default view ports and scissors
			.m_blendConstants = {},
			.m_pushConstants = {},
			.m_currentFrame = m_vkState().m_currentFrame
			});

		vvh::ShadowOffset shadOff{ .shadowIndexOffset = 0, .numberShadows = 0 };

		uint32_t numberTotalLayers = shadowImage().numberImageArraylayers;
		for (uint32_t layer = 0; layer < numberTotalLayers; ++layer) {

			vkCmdPushConstants(cmdBuffer,
				m_shadowPipeline.m_pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT,
				0,
				sizeof(vvh::ShadowOffset),
				&shadOff
			);

			for (auto [oHandle, name, ghandle, LtoW, uniformBuffers, descriptorsets] :
				m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vvh::Buffer&, vvh::DescriptorSet&>
				({ (size_t)m_shadowPipeline.m_pipeline })) {

				const vvh::Mesh& mesh = m_registry.template Get<vvh::Mesh&>(ghandle);

				vvh::ComRecordObject({
					.m_commandBuffer = cmdBuffer,
					.m_graphicsPipeline = m_shadowPipeline,
					.m_descriptorSets = { m_descriptorSetPerFrame, descriptorsets },
					.m_type = "P",
					.m_mesh = mesh,
					.m_currentFrame = m_vkState().m_currentFrame
					});
			}

		}

		vvh::ComEndRenderPass({ .m_commandBuffer = cmdBuffer });
		vvh::ComEndCommandBuffer({ .m_commandBuffer = cmdBuffer });
		SubmitCommandBuffer(cmdBuffer);

		// TODO: write logic for when shadow is calculated (start, and then only when light changes)
		// TODO: Write own Message maybe?
		//m_engine.DeregisterCallbacks(this, "PREPARE_NEXT_FRAME");
		//m_engine.DeregisterCallbacks(this, "RECORD_NEXT_FRAME");

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


};   // namespace vve