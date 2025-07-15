#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	template class RendererDeferredCommon<RendererDeferred11>;
	template class RendererDeferredCommon<RendererDeferred13>;

	template<typename Derived>
	RendererDeferredCommon<Derived>::RendererDeferredCommon(const std::string& systemName, Engine& engine, const std::string& windowName) : Renderer(systemName, engine, windowName) {

		engine.RegisterCallbacks({
			{this,  3500, "INIT",				[this](Message& message) { return OnInit(message); } },
			{this,  2000, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); } },
			{this,  2000, "RECORD_NEXT_FRAME",	[this](Message& message) { return OnRecordNextFrame(message); } },
			{this,  1750, "OBJECT_CREATE",		[this](Message& message) { return OnObjectCreate(message); } },
			{this, 10000, "OBJECT_DESTROY",		[this](Message& message) { return OnObjectDestroy(message); } },
			{this,  1500, "WINDOW_SIZE",		[this](Message& message) { return OnWindowSize(message); }},
			{this, 	   0, "QUIT",				[this](Message& message) { return OnQuit(message); } }
			});
	}

	template<typename Derived>
	RendererDeferredCommon<Derived>::~RendererDeferredCommon() {};


	template<typename Derived>
	bool RendererDeferredCommon<Derived>::OnInit(const Message& message) {
		Renderer::OnInit(message);

		// Per Frame
		vvh::RenCreateDescriptorSetLayout({
			.m_device = m_vkState().m_device,
			.m_bindings = {
				{	// Binding 0 : Vertex and Fragment uniform buffer
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 1 : Light uniform buffer
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			.m_descriptorSetLayout = m_descriptorSetLayoutPerFrame
			});

		// Composition
		vvh::RenCreateDescriptorSetLayout({
			.m_device = m_vkState().m_device,
			.m_bindings = {
				{	// Binding 0 : Normal
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 1 : Albedo
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 2 : Metallic and Roughness
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 3 : Depth
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			.m_descriptorSetLayout = m_descriptorSetLayoutComposition
			});

		// Shadow
		vvh::RenCreateDescriptorSetLayout({
			.m_device = m_vkState().m_device,
			.m_bindings = {
				{	// Binding 0 : ShadowMap Cube Array - Point Lights
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 1 : ShadowMap 2D Array - Direct + Spot Lights
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 2 : Light Space Matrix - Direct + Spot Lights
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			.m_descriptorSetLayout = m_descriptorSetLayoutShadow
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

		// TODO: shrink pool to only what is needed - why 1000?
		vvh::RenCreateDescriptorPool({
			.m_device = m_vkState().m_device,
			.m_sizes = 1000,
			.m_descriptorPool = m_descriptorPool
			});
		vvh::RenCreateDescriptorSet({	// Per Frame
			.m_device = m_vkState().m_device,
			.m_descriptorSetLayouts = m_descriptorSetLayoutPerFrame,
			.m_descriptorPool = m_descriptorPool,
			.m_descriptorSet = m_descriptorSetPerFrame
			});
		vvh::RenCreateDescriptorSet({	// Composition
			.m_device = m_vkState().m_device,
			.m_descriptorSetLayouts = m_descriptorSetLayoutComposition,
			.m_descriptorPool = m_descriptorPool,
			.m_descriptorSet = m_descriptorSetComposition
			});
		vvh::RenCreateDescriptorSet({	// Shadow
			.m_device = m_vkState().m_device,
			.m_descriptorSetLayouts = m_descriptorSetLayoutShadow,
			.m_descriptorPool = m_descriptorPool,
			.m_descriptorSet = m_descriptorSetShadow
			});

		// Per frame uniform buffer
		vvh::BufCreateBuffers({
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			.m_size = sizeof(vvh::UniformBufferFrame),
			.m_buffer = m_uniformBuffersPerFrame
			});
		vvh::RenUpdateDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_uniformBuffers = m_uniformBuffersPerFrame,
			.m_binding = 0,
			.m_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.m_size = sizeof(vvh::UniformBufferFrame),
			.m_descriptorSet = m_descriptorSetPerFrame
			});

		// Per frame light buffer
		vvh::BufCreateBuffers({
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.m_size = MAX_NUMBER_LIGHTS * sizeof(vvh::Light),
			.m_buffer = m_storageBuffersLights
			});
		vvh::RenUpdateDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_uniformBuffers = m_storageBuffersLights,
			.m_binding = 1,
			.m_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.m_size = MAX_NUMBER_LIGHTS * sizeof(vvh::Light),
			.m_descriptorSet = m_descriptorSetPerFrame
			});

		// Composition Sampler
		vvh::ImgCreateImageSampler({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_sampler = m_sampler,
			.m_filter = VK_FILTER_LINEAR,
			});
		// Sampler with anisotropy enabled for albedo
		vvh::ImgCreateImageSampler({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_sampler = m_albedoSampler,
			.m_filter = VK_FILTER_LINEAR,
			.m_anisotropyEnable = VK_TRUE
			});

		// Shadow Sampler
		vvh::ImgCreateImageSampler({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_sampler = m_shadowSampler,
			.m_filter = VK_FILTER_LINEAR,
			.m_borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			.m_compareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
			.m_compareEnable = VK_TRUE,
			.m_maxLod = 0.0f,
			});

		// Shadow light space matrices
		vvh::BufCreateBuffers({
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			.m_size = MAX_NUMBER_LIGHTS * sizeof(glm::mat4),
			.m_buffer = m_storageBuffersLightSpaceMatrices
			});
		vvh::RenUpdateDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_uniformBuffers = m_storageBuffersLightSpaceMatrices,
			.m_binding = 2,
			.m_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.m_size = MAX_NUMBER_LIGHTS * sizeof(glm::mat4),
			.m_descriptorSet = m_descriptorSetShadow
			});

		CreateDeferredResources();

		static_cast<Derived*>(this)->OnInit();

		return false;
	}

	template<typename Derived>
	bool RendererDeferredCommon<Derived>::OnPrepareNextFrame(const Message& message) {

		vvh::UniformBufferFrame ubc;
		vkResetCommandPool(m_vkState().m_device, m_commandPools[m_vkState().m_currentFrame], 0);

		if (m_lightsChanged) {
			m_lightsChanged = false;	
			UpdateLightStorageBuffer();
		}
		ubc.numLights = m_numberLightsPerType;

		auto [lToW, view, proj] = *m_registry.template GetView<LocalToWorldMatrix&, ViewMatrix&, ProjectionMatrix&>().begin();
		ubc.camera.view = view();
		ubc.camera.proj = proj();
		ubc.camera.positionW = lToW()[3];
		memcpy(m_uniformBuffersPerFrame.m_uniformBuffersMapped[m_vkState().m_currentFrame], &ubc, sizeof(ubc));

		for (const auto& pipeline : m_geomPipesPerType) {
			for (auto [oHandle, name, ghandle, LtoW, uniformBuffers] :
				m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vvh::Buffer&>({ (size_t)pipeline.second.m_graphicsPipeline.m_pipeline })) {

				bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
				bool hasColor = m_registry.template Has<vvh::Color>(oHandle);
				bool hasVertexColor = pipeline.second.m_type.find("C") != std::string::npos;
				if (!hasTexture && !hasColor && !hasVertexColor) continue;

				if (hasTexture) {
					vvh::BufferPerObjectTexture uboTexture{};
					uboTexture.model = LtoW();
					uboTexture.modelInverseTranspose = glm::inverse(glm::transpose(uboTexture.model));
					UVScale uvScale{ { 1.0f, 1.0f } };
					if (m_registry.template Has<UVScale>(oHandle)) { uvScale = m_registry.template Get<UVScale>(oHandle); }
					uboTexture.uvScale = uvScale;
					memcpy(uniformBuffers().m_uniformBuffersMapped[m_vkState().m_currentFrame], &uboTexture, sizeof(uboTexture));
				}
				else if (hasColor) {
					vvh::BufferPerObjectColor uboColor{};
					uboColor.model = LtoW();
					uboColor.modelInverseTranspose = glm::inverse(glm::transpose(uboColor.model));
					uboColor.color = m_registry.template Get<vvh::Color>(oHandle);
					memcpy(uniformBuffers().m_uniformBuffersMapped[m_vkState().m_currentFrame], &uboColor, sizeof(uboColor));
				}
				else if (hasVertexColor) {
					vvh::BufferPerObject uboColor{};
					uboColor.model = LtoW();
					uboColor.modelInverseTranspose = glm::inverse(glm::transpose(uboColor.model));
					memcpy(uniformBuffers().m_uniformBuffersMapped[m_vkState().m_currentFrame], &uboColor, sizeof(uboColor));
				}
			}
		}

		static_cast<Derived*>(this)->OnPrepareNextFrame();

		return false;
	}

	template<typename Derived>
	bool RendererDeferredCommon<Derived>::OnRecordNextFrame(const Message& message) {

		if (m_shadowsNeedUpdate && m_engine.IsShadowEnabled()) {
			auto [sHandle, shadowImage] = *m_registry.template GetView<vecs::Handle, ShadowImage&>().begin();
			// shadow cube array for point lights
			vvh::RenUpdateImageDescriptorSet({
				.m_device = m_vkState().m_device,
				.m_imageView = shadowImage().m_cubeArrayView,
				.m_sampler = m_shadowSampler,
				.m_binding = 0,
				.m_descriptorSet = m_descriptorSetShadow,
				.m_descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				});
			// shadow 2D array for direct + spot lights
			vvh::RenUpdateImageDescriptorSet({
				.m_device = m_vkState().m_device,
				.m_imageView = shadowImage().m_2DArrayView,
				.m_sampler = m_shadowSampler,
				.m_binding = 1,
				.m_descriptorSet = m_descriptorSetShadow,
				.m_descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				});

			size_t lsmSize = shadowImage().m_lightSpaceMatrices.size() * sizeof(glm::mat4);
			for (size_t i = 0; i < m_storageBuffersLightSpaceMatrices.m_uniformBuffersMapped.size(); ++i) {
				memcpy(m_storageBuffersLightSpaceMatrices.m_uniformBuffersMapped[i], shadowImage().m_lightSpaceMatrices.data(), lsmSize);
			}

			m_shadowsNeedUpdate = false;
		}

		static_cast<Derived*>(this)->OnRecordNextFrame();

		return false;
	}

	template<typename Derived>
	bool RendererDeferredCommon<Derived>::OnObjectCreate(Message& message) {
		const ObjectHandle& oHandle = message.template GetData<MsgObjectCreate>().m_object;
		assert(m_registry.template Has<MeshHandle>(oHandle));

		if (m_registry.template Has<PointLight>(oHandle) ||
			m_registry.template Has<DirectionalLight>(oHandle) ||
			m_registry.template Has<SpotLight>(oHandle)) {
			// Object is a light, update m_storageBuffersLights in OnPrepareNextFrame!
			m_lightsChanged = true;
		}

		const auto& meshHandle = m_registry.template Get<MeshHandle>(oHandle);
		const vvh::Mesh& mesh = m_registry.template Get<vvh::Mesh&>(meshHandle);
		const auto& type = getPipelineType(oHandle, mesh.m_verticesData);
		const auto& pipelinePerType = getPipelinePerType(type);

		bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
		bool hasColor = m_registry.template Has<vvh::Color>(oHandle);
		bool hasVertexColor = pipelinePerType->m_type.find("C") != std::string::npos;
		if (!hasTexture && !hasColor && !hasVertexColor) return false;

		vvh::Buffer ubo;
		size_t sizeUbo = 0;
		vvh::DescriptorSet descriptorSet{ 1 };
		vvh::RenCreateDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_descriptorSetLayouts = pipelinePerType->m_descriptorSetLayoutPerObject,
			.m_descriptorPool = m_descriptorPool,
			.m_descriptorSet = descriptorSet
			});

		if (hasTexture) {
			sizeUbo = sizeof(vvh::BufferPerObjectTexture);
			const auto& tHandle = m_registry.template Get<TextureHandle>(oHandle);
			const vvh::Image& texture = m_registry.template Get<vvh::Image&>(tHandle);
			vvh::RenUpdateDescriptorSetTexture({
				.m_device = m_vkState().m_device,
				.m_texture = texture,
				.m_binding = 1,
				.m_descriptorSet = descriptorSet
				});
		}
		else if (hasColor) {
			sizeUbo = sizeof(vvh::BufferPerObjectColor);
		}
		else if (hasVertexColor) {
			sizeUbo = sizeof(vvh::BufferPerObject);
		}

		vvh::BufCreateBuffers({
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			.m_size = sizeUbo,
			.m_buffer = ubo
			});
		vvh::RenUpdateDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_uniformBuffers = ubo,
			.m_binding = 0,
			.m_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.m_size = sizeUbo,
			.m_descriptorSet = descriptorSet
			});

		m_registry.Put(oHandle, ubo, descriptorSet);
		m_registry.AddTags(oHandle, (size_t)pipelinePerType->m_graphicsPipeline.m_pipeline);

		assert(m_registry.template Has<vvh::Buffer>(oHandle));
		assert(m_registry.template Has<vvh::DescriptorSet>(oHandle));


		static_cast<Derived*>(this)->OnObjectCreate();

		return false;
	}

	template<typename Derived>
	bool RendererDeferredCommon<Derived>::OnObjectDestroy(Message& message) {
		const auto& msg = message.template GetData<MsgObjectDestroy>();
		const auto& oHandle = msg.m_handle();

		assert(m_registry.Exists(oHandle));

		if (m_registry.template Has<PointLight>(oHandle) ||
			m_registry.template Has<DirectionalLight>(oHandle) ||
			m_registry.template Has<SpotLight>(oHandle)) {
			// Object is a light, update m_storageBuffersLights in OnPrepareNextFrame!
			m_lightsChanged = true;
		}

		if (m_registry.template Has<vvh::Buffer>(oHandle)) {
			vvh::Buffer& ubo = m_registry.template Get<vvh::Buffer&>(oHandle);
			vvh::BufDestroyBuffer2({
				.m_device = m_vkState().m_device,
				.m_vmaAllocator = m_vkState().m_vmaAllocator,
				.m_buffers = ubo
				});
		}

		if (m_registry.template Has<vvh::DescriptorSet&>(oHandle)) {
			vvh::DescriptorSet& vvh_ds = m_registry.template Get<vvh::DescriptorSet&>(oHandle);
			for (auto& ds : vvh_ds.m_descriptorSetPerFrameInFlight) {
				vkFreeDescriptorSets(m_vkState().m_device, m_descriptorPool, 1, &ds);
			}
		}

		static_cast<Derived*>(this)->OnObjectDestroy();

		return false;
	}

	template<typename Derived>
	bool RendererDeferredCommon<Derived>::OnWindowSize(const Message& message) {
		DestroyDeferredResources();
		CreateDeferredResources();

		static_cast<Derived*>(this)->OnWindowSize();

		return false;
	}

	template<typename Derived>
	bool RendererDeferredCommon<Derived>::OnQuit(const Message& message) {
		vkDeviceWaitIdle(m_vkState().m_device);

		for (auto pool : m_commandPools) {
			vkDestroyCommandPool(m_vkState().m_device, pool, nullptr);
		}

		for (auto& [type, pipeline] : m_geomPipesPerType) {
			vkDestroyDescriptorSetLayout(m_vkState().m_device, pipeline.m_descriptorSetLayoutPerObject, nullptr);
			vkDestroyPipeline(m_vkState().m_device, pipeline.m_graphicsPipeline.m_pipeline, nullptr);
			vkDestroyPipelineLayout(m_vkState().m_device, pipeline.m_graphicsPipeline.m_pipelineLayout, nullptr);
		}
		vkDestroyPipeline(m_vkState().m_device, m_lightingPipeline.m_pipeline, nullptr);
		vkDestroyPipelineLayout(m_vkState().m_device, m_lightingPipeline.m_pipelineLayout, nullptr);

		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerFrame, nullptr);
		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutComposition, nullptr);

		vkDestroyDescriptorPool(m_vkState().m_device, m_descriptorPool, nullptr);

		vkDestroySampler(m_vkState().m_device, m_sampler, nullptr);
		vkDestroySampler(m_vkState().m_device, m_albedoSampler, nullptr);
		vkDestroySampler(m_vkState().m_device, m_shadowSampler, nullptr);

		for (auto& buffer : { std::ref(m_uniformBuffersPerFrame), std::ref(m_storageBuffersLights) }) {
			vvh::BufDestroyBuffer2({
				.m_device = m_vkState().m_device,
				.m_vmaAllocator = m_vkState().m_vmaAllocator,
				.m_buffers = buffer.get()
				});
		}

		DestroyDeferredResources();


		static_cast<Derived*>(this)->OnQuit();

		return false;
	}

	template<typename Derived>
	void RendererDeferredCommon<Derived>::CreateDeferredResources() {
		m_gBufferAttachments.resize(COUNT - 1);

		// Normal
		vvh::RenCreateGBufferResources({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_swapChain = m_vkState().m_swapChain,
			.m_gbufferImage = m_gBufferAttachments[NORMAL],
			.m_format = VK_FORMAT_R16G16B16A16_SFLOAT,
			.m_sampler = m_sampler
			});
		vvh::RenUpdateImageDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_imageView = m_gBufferAttachments[NORMAL].m_gbufferImageView,
			.m_sampler = m_sampler,
			.m_binding = NORMAL,
			.m_descriptorSet = m_descriptorSetComposition
			});
		// Albedo
		vvh::RenCreateGBufferResources({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_swapChain = m_vkState().m_swapChain,
			.m_gbufferImage = m_gBufferAttachments[ALBEDO],
			.m_format = VK_FORMAT_R8G8B8A8_SRGB,
			.m_sampler = m_albedoSampler
			});
		vvh::RenUpdateImageDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_imageView = m_gBufferAttachments[ALBEDO].m_gbufferImageView,
			.m_sampler = m_albedoSampler,
			.m_binding = ALBEDO,
			.m_descriptorSet = m_descriptorSetComposition
			});
		// Metallic and Roughness
		vvh::RenCreateGBufferResources({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_swapChain = m_vkState().m_swapChain,
			.m_gbufferImage = m_gBufferAttachments[METALLIC_ROUGHNESS],
			.m_format = VK_FORMAT_R8G8B8A8_UNORM,
			.m_sampler = m_sampler
			});
		vvh::RenUpdateImageDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_imageView = m_gBufferAttachments[METALLIC_ROUGHNESS].m_gbufferImageView,
			.m_sampler = m_sampler,
			.m_binding = METALLIC_ROUGHNESS,
			.m_descriptorSet = m_descriptorSetComposition
			});
		// Depth
		vvh::RenUpdateDescriptorSetDepthAttachment({
			.m_device = m_vkState().m_device,
			.m_depthImage = m_vkState().m_depthImage,
			.m_binding = DEPTH,
			.m_descriptorSet = m_descriptorSetComposition,
			.m_sampler = m_sampler
			});
		// ShadowMap
		// Descriptor is updated once the shadowMap is built (currently in OnRecordFrame)

		// GBuffer attachments from  VK_IMAGE_LAYOUT_UNDEFINED --> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		for (auto& image : m_gBufferAttachments) {
			vvh::ImgTransitionImageLayout3({
				.m_device = m_vkState().m_device,
				.m_graphicsQueue = m_vkState().m_graphicsQueue,
				.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
				.m_image = image.m_gbufferImage,
				.m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.m_newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.m_commandBuffer = VK_NULL_HANDLE
				});
		}
	}

	template<typename Derived>
	void RendererDeferredCommon<Derived>::DestroyDeferredResources() {
		for (auto& gBufferAttach : m_gBufferAttachments) {
			vvh::ImgDestroyImage({
				.m_device = m_vkState().m_device,
				.m_vmaAllocator = m_vkState().m_vmaAllocator,
				.m_image = gBufferAttach.m_gbufferImage,
				.m_imageAllocation = gBufferAttach.m_gbufferImageAllocation
				});
			vkDestroyImageView(m_vkState().m_device, gBufferAttach.m_gbufferImageView, nullptr);
		}
	}

	template<typename Derived>
	void RendererDeferredCommon<Derived>::CreateGeometryPipeline(const VkRenderPass* renderPass) {
		const std::filesystem::path shaders{ "shaders/Deferred" };
		if (!std::filesystem::exists(shaders)) {
			std::cerr << "ERROR: Folder does not exist: " << std::filesystem::absolute(shaders) << "\n";
		}

		for (const auto& entry : std::filesystem::directory_iterator(shaders)) {
			auto filename = entry.path().filename().string();
			if (filename.find(".spv") != std::string::npos && std::isdigit(filename[0])) {
				size_t pos1 = filename.find("_");
				size_t pos2 = filename.find(".spv");
				auto pri = std::stoi(filename.substr(0, pos1 - 1));
				std::string type = filename.substr(pos1 + 1, pos2 - pos1 - 1);

				vvh::Pipeline graphicsPipeline{};

				VkDescriptorSetLayout descriptorSetLayoutPerObject{};
				std::vector<VkDescriptorSetLayoutBinding> bindings{
					{.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT }
				};

				if (type.find("U") != std::string::npos) { //texture map
					bindings.push_back({ .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT });
				}

				vvh::RenCreateDescriptorSetLayout({
					.m_device = m_vkState().m_device,
					.m_bindings = bindings,
					.m_descriptorSetLayout = descriptorSetLayoutPerObject
					});

				std::vector<VkVertexInputBindingDescription> bindingDescriptions = getBindingDescriptions(type);
				std::vector<VkVertexInputAttributeDescription> attributeDescriptions = getAttributeDescriptions(type);

				VkPipelineColorBlendAttachmentState colorBlendAttachment{};
				colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
					| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
				colorBlendAttachment.blendEnable = VK_FALSE;

				std::vector<VkPipelineColorBlendAttachmentState> blends{};
				blends.reserve(COUNT - 1);
				for (uint8_t i = 0; i < COUNT - 1; ++i) blends.push_back(colorBlendAttachment);

				vvh::RenCreateGraphicsPipeline({
					.m_device = m_vkState().m_device,
					.m_renderPass = renderPass != VK_NULL_HANDLE ? *renderPass : VK_NULL_HANDLE,
					.m_vertShaderPath = entry.path().string(),
					.m_fragShaderPath = entry.path().string(),
					.m_bindingDescription = bindingDescriptions,
					.m_attributeDescriptions = attributeDescriptions,
					.m_descriptorSetLayouts = { m_descriptorSetLayoutPerFrame, descriptorSetLayoutPerObject },
					.m_specializationConstants = {},
					.m_pushConstantRanges = { {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(PushConstantsMaterial) } },
					.m_blendAttachments = blends,
					.m_graphicsPipeline = graphicsPipeline,
					.m_attachmentFormats = getAttachmentFormats(),
					.m_depthFormat = m_vkState().m_depthMapFormat,
					.m_depthWrite = true
					});

				m_geomPipesPerType[pri] = { type, descriptorSetLayoutPerObject, graphicsPipeline };
			}
		}
	}

	template<typename Derived>
	void RendererDeferredCommon<Derived>::CreateLightingPipeline(const VkRenderPass* renderPass) {
		const std::filesystem::path shaders{ "shaders/Deferred" };
		if (!std::filesystem::exists(shaders)) {
			std::cerr << "ERROR: Folder does not exist: " << std::filesystem::absolute(shaders) << "\n";
		}
		const std::string vert = (shaders / "PBR_lighting.spv").string();
		const std::string frag = (shaders / "PBR_lighting.spv").string();

		vvh::RenCreateGraphicsPipeline({
			.m_device = m_vkState().m_device,
			.m_renderPass = renderPass != VK_NULL_HANDLE ? *renderPass : VK_NULL_HANDLE,
			.m_vertShaderPath = vert,
			.m_fragShaderPath = frag,
			.m_bindingDescription = {},
			.m_attributeDescriptions = {},
			.m_descriptorSetLayouts = { m_descriptorSetLayoutPerFrame, m_descriptorSetLayoutComposition, m_descriptorSetLayoutShadow },
			.m_specializationConstants = { MAX_NUMBER_LIGHTS },
			.m_pushConstantRanges = { {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(PushConstantsLight) } },
			.m_blendAttachments = {},
			.m_graphicsPipeline = m_lightingPipeline,
			.m_attachmentFormats = { m_vkState().m_swapChain.m_swapChainImageFormat },
			.m_depthFormat = m_vkState().m_depthMapFormat,
			.m_depthWrite = false
			});
	}

	template<typename Derived>
	auto RendererDeferredCommon<Derived>::getAttachmentFormats() const -> const std::vector<VkFormat> {
		std::vector<VkFormat> attachFormats;
		attachFormats.reserve(m_gBufferAttachments.size());
		for (const auto& attach : m_gBufferAttachments) {
			attachFormats.emplace_back(attach.m_gbufferFormat);
		}

		return attachFormats;
	}

	template<typename Derived>
	auto RendererDeferredCommon<Derived>::getPipelineType(const ObjectHandle& handle, const vvh::VertexData& vertexData) const -> const std::string {
		std::string type = vertexData.getType();
		if (m_registry.template Has<TextureHandle>(handle) && type.find("U") != std::string::npos) type += "E";
		if (m_registry.template Has<vvh::Color>(handle) && type.find("C") == std::string::npos && type.find("E") == std::string::npos) type += "O";
		return type;
	}

	template<typename Derived>
	auto RendererDeferredCommon<Derived>::getPipelinePerType(const std::string& type) const -> const RendererDeferredCommon<Derived>::PipelinePerType* {
		for (auto& [pri, pipeline] : m_geomPipesPerType) {
			bool found = true;
			for (auto& c : pipeline.m_type) { found = found && (type.find(c) != std::string::npos); }
			if (found) return &pipeline;
		}
		std::cout << "Pipeline not found for type: " << type << std::endl;
		exit(-1);
		return nullptr;
	}

	template<typename Derived>
	void RendererDeferredCommon<Derived>::UpdateLightStorageBuffer() {
		static std::vector<vvh::Light> lights{ MAX_NUMBER_LIGHTS };
		int total = 0;
		
		m_numberLightsPerType = glm::ivec3{ 0 };
		m_numberLightsPerType.x = RegisterLight<PointLight>(1.0f, lights, total);
		m_numberLightsPerType.y = RegisterLight<DirectionalLight>(2.0f, lights, total);
		m_numberLightsPerType.z = RegisterLight<SpotLight>(3.0f, lights, total);

		for (size_t i = 0; i < m_storageBuffersLights.m_uniformBuffersMapped.size(); ++i) {
			memcpy(m_storageBuffersLights.m_uniformBuffersMapped[i], lights.data(), total * sizeof(vvh::Light));
		}

		m_shadowsNeedUpdate = true;
	}

	template<typename Derived>
	void RendererDeferredCommon<Derived>::PrepareLightingAttachments(const VkCommandBuffer& cmdBuffer) {
		// GBuffer attachments VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL --> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		for (auto& image : m_gBufferAttachments) {
			vvh::ImgTransitionImageLayout3({
				.m_device = m_vkState().m_device,
				.m_graphicsQueue = m_vkState().m_graphicsQueue,
				.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
				.m_image = image.m_gbufferImage,
				.m_oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.m_newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.m_commandBuffer = cmdBuffer
				});
		}

		// Depth image VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL --> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		vvh::ImgTransitionImageLayout3({
			.m_device = m_vkState().m_device,
			.m_graphicsQueue = m_vkState().m_graphicsQueue,
			.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
			.m_image = m_vkState().m_depthImage.m_depthImage,
			.m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
			.m_oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.m_newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.m_commandBuffer = cmdBuffer
		});
	}

	template<typename Derived>
	void RendererDeferredCommon<Derived>::ResetLightingAttachments(const VkCommandBuffer& cmdBuffer) {
		// GBuffer attachments from  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL --> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		for (auto& image : m_gBufferAttachments) {
			vvh::ImgTransitionImageLayout3({
				.m_device = m_vkState().m_device,
				.m_graphicsQueue = m_vkState().m_graphicsQueue,
				.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
				.m_image = image.m_gbufferImage,
				.m_oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.m_newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.m_commandBuffer = cmdBuffer
				});
		}

		// Depth image VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL --> VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		vvh::ImgTransitionImageLayout3({
			.m_device = m_vkState().m_device,
			.m_graphicsQueue = m_vkState().m_graphicsQueue,
			.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
			.m_image = m_vkState().m_depthImage.m_depthImage,
			.m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
			.m_oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.m_newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.m_commandBuffer = cmdBuffer
			});
	}

	template<typename Derived>
	void RendererDeferredCommon<Derived>::RecordObjects(const VkCommandBuffer& cmdBuffer, const VkRenderPass* renderPass) {
		assert(sizeof(PushConstantsMaterial) <= 128);
		PushConstantsMaterial lastPush{ {std::numeric_limits<int>::min(), std::numeric_limits<int>::min()} };

		for (const auto& pipeline : m_geomPipesPerType) {

			vvh::Pipeline pip{
				pipeline.second.m_graphicsPipeline.m_pipelineLayout,
				pipeline.second.m_graphicsPipeline.m_pipeline
			};

			vvh::ComBindPipeline({
				.m_commandBuffer = cmdBuffer,
				.m_graphicsPipeline = pip,
				.m_extent = m_vkState().m_swapChain.m_swapChainExtent,
				.m_viewPorts = {},
				.m_scissors = {}, //default view ports and scissors
				.m_blendConstants = {},
				.m_pushConstants = {}
				});

			for (auto [oHandle, name, ghandle, LtoW, uniformBuffers, descriptorset] :
				m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vvh::Buffer&, vvh::DescriptorSet&>
				({ (size_t)pipeline.second.m_graphicsPipeline.m_pipeline })) {

				if (m_registry.template Has<PointLight>(oHandle) || m_registry.template Has<SpotLight>(oHandle)) {
					// Does not render the point or spot light sphere - has to be removed if this wants to be used
					continue;
				}

				bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
				bool hasColor = m_registry.template Has<vvh::Color>(oHandle);
				bool hasVertexColor = pipeline.second.m_type.find("C") != std::string::npos;
				if (!hasTexture && !hasColor && !hasVertexColor) continue;

				const vvh::Mesh& mesh = m_registry.template Get<vvh::Mesh&>(ghandle);
				
				// Metallic and Roughness as push constants per object
				PushConstantsMaterial currentMetalRough = [&]() {
					bool hasMaterial = m_registry.template Has<vvh::Material>(oHandle);
					if (hasMaterial) {
						const vvh::Material& material = m_registry.template Get<vvh::Material&>(oHandle);
						return PushConstantsMaterial{ {material.m_material.r, material.m_material.g} };
					}
					else return PushConstantsMaterial{};	// default values
				}();

				if (currentMetalRough.m_metallRoughness != lastPush.m_metallRoughness) {
					vkCmdPushConstants(cmdBuffer,
						pipeline.second.m_graphicsPipeline.m_pipelineLayout,
						VK_SHADER_STAGE_FRAGMENT_BIT,
						0,
						sizeof(PushConstantsMaterial),
						&currentMetalRough
					);
					lastPush = currentMetalRough;
				}

				vvh::ComRecordObject({
					.m_commandBuffer = cmdBuffer,
					.m_graphicsPipeline = pipeline.second.m_graphicsPipeline,
					.m_descriptorSets = { m_descriptorSetPerFrame, descriptorset },
					.m_type = pipeline.second.m_type,
					.m_mesh = mesh,
					.m_currentFrame = m_vkState().m_currentFrame
					});
			}
		}
	}

	template<typename Derived>
	void RendererDeferredCommon<Derived>::RecordLighting(const VkCommandBuffer& cmdBuffer, const VkRenderPass* renderPass) {
		assert(sizeof(PushConstantsLight) <= 128);

		// TODO: Maybe move into camera ubo
		auto [view, proj] = *m_registry.template GetView<ViewMatrix&, ProjectionMatrix&>().begin();
		PushConstantsLight pc{ .m_invViewProj = glm::inverse(proj() * view()) };

		vvh::ComBindPipeline({
			.m_commandBuffer = cmdBuffer,
			.m_graphicsPipeline = m_lightingPipeline,
			.m_extent = m_vkState().m_swapChain.m_swapChainExtent,
			.m_viewPorts = {},
			.m_scissors = {}, //default view ports and scissors
			.m_blendConstants = {},
			.m_pushConstants = {
				{
					.layout = m_lightingPipeline.m_pipelineLayout,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.offset = 0,
					.size = sizeof(PushConstantsLight),
					.pValues = &pc
				}
			}
			});

		vvh::ComRecordLighting({
			.m_commandBuffer = cmdBuffer,
			.m_graphicsPipeline = m_lightingPipeline,
			.m_descriptorSets = { m_descriptorSetPerFrame, m_descriptorSetComposition, m_descriptorSetShadow },
			.m_currentFrame = m_vkState().m_currentFrame
			});
	}

}	// namespace vve
