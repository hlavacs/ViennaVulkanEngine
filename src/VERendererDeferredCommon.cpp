#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	template class RendererDeferredCommon<RendererDeferred11>;
	template class RendererDeferredCommon<RendererDeferred13>;

	template<typename Derived>
	RendererDeferredCommon<Derived>::RendererDeferredCommon(std::string systemName, Engine& engine, std::string windowName) : Renderer(systemName, engine, windowName) {

		engine.RegisterCallbacks({
			{this,  3500, "INIT",				[this](Message& message) { return OnInit(message); } },
			{this,  2000, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); } },
			});
	}

	template<typename Derived>
	RendererDeferredCommon<Derived>::~RendererDeferredCommon() {};


	template<typename Derived>
	bool RendererDeferredCommon<Derived>::OnInit(Message message) {
		// TODO: maybe a reference will be enough here to not make a message copy?
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
				{	// Binding 0 : Position
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 1 : Normal
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 2 : Albedo
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 3 : Depth
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
			},
			.m_descriptorSetLayout = m_descriptorSetLayoutComposition
			});

		m_commandPools.resize(MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vvh::ComCreateCommandPool({
				.m_surface = m_vkState().m_surface,
				.m_physicalDevice = m_vkState().m_physicalDevice,
				.m_device = m_vkState().m_device,
				.m_commandPool = m_commandPools[i]
				});
		}

		m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		vvh::ComCreateCommandBuffers({
				.m_device = m_vkState().m_device,
				.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
				.m_commandBuffers = m_commandBuffers
			});

		// TODO: shrink pool to only what is needed - why 1000?
		vvh::RenCreateDescriptorPool({
			.m_device = m_vkState().m_device,
			.m_sizes = 1000,
			.m_descriptorPool = m_descriptorPool
			});
		vvh::RenCreateDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_descriptorSetLayouts = m_descriptorSetLayoutPerFrame,
			.m_descriptorPool = m_descriptorPool,
			.m_descriptorSet = m_descriptorSetPerFrame
			});
		vvh::RenCreateDescriptorSet({
			.m_device = m_vkState().m_device,
			.m_descriptorSetLayouts = m_descriptorSetLayoutComposition,
			.m_descriptorPool = m_descriptorPool,
			.m_descriptorSet = m_descriptorSetComposition
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
			.m_sampler = m_sampler
			});

		CreateDeferredResources();

		static_cast<Derived*>(this)->OnInit(message);

		return false;
	}

	template<typename Derived>
	bool RendererDeferredCommon<Derived>::OnPrepareNextFrame(Message message) {
		m_pass = 0;

		vvh::UniformBufferFrame ubc;
		vkResetCommandPool(m_vkState().m_device, m_commandPools[m_vkState().m_currentFrame], 0);

		int total{ 0 };
		std::vector<vvh::Light> lights{ MAX_NUMBER_LIGHTS };
		// TODO: Maybe move into separate function to not do this every frame?
		m_numberLightsPerType = glm::ivec3{ 0 };
		m_numberLightsPerType.x = RegisterLight<PointLight>(1.0f, lights, total);
		m_numberLightsPerType.y = RegisterLight<DirectionalLight>(2.0f, lights, total);
		m_numberLightsPerType.z = RegisterLight<SpotLight>(3.0f, lights, total);
		ubc.numLights = m_numberLightsPerType;
		memcpy(m_storageBuffersLights.m_uniformBuffersMapped[m_vkState().m_currentFrame], lights.data(), total * sizeof(vvh::Light));

		auto [lToW, view, proj] = *m_registry.template GetView<LocalToWorldMatrix&, ViewMatrix&, ProjectionMatrix&>().begin();
		ubc.camera.view = view();
		ubc.camera.proj = proj();
		ubc.camera.positionW = lToW()[3];
		memcpy(m_uniformBuffersPerFrame.m_uniformBuffersMapped[m_vkState().m_currentFrame], &ubc, sizeof(ubc));

		for (auto& pipeline : m_geomPipesPerType) {
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

		static_cast<Derived*>(this)->OnPrepareNextFrame(message);

		return false;
	}

	template<typename Derived>
	void RendererDeferredCommon<Derived>::CreateDeferredResources() {

		vvh::RenCreateGBufferResources({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_swapChain = m_vkState().m_swapChain,
			.m_gbufferImage = m_gBufferAttachments[POSITION],
			.m_format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.m_sampler = m_sampler
			});
		vvh::RenUpdateDescriptorSetGBufferAttachment({
			.m_device = m_vkState().m_device,
			.m_gbufferImage = m_gBufferAttachments[POSITION],
			.m_binding = POSITION,
			.m_descriptorSet = m_descriptorSetComposition
			});
		vvh::RenCreateGBufferResources({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_swapChain = m_vkState().m_swapChain,
			.m_gbufferImage = m_gBufferAttachments[NORMAL],
			.m_format = VK_FORMAT_R8G8B8A8_UNORM,
			.m_sampler = m_sampler
			});
		vvh::RenUpdateDescriptorSetGBufferAttachment({
			.m_device = m_vkState().m_device,
			.m_gbufferImage = m_gBufferAttachments[NORMAL],
			.m_binding = NORMAL,
			.m_descriptorSet = m_descriptorSetComposition
			});
		vvh::RenCreateGBufferResources({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_swapChain = m_vkState().m_swapChain,
			.m_gbufferImage = m_gBufferAttachments[ALBEDO],
			.m_format = VK_FORMAT_R8G8B8A8_SRGB,
			.m_sampler = m_sampler
			});
		vvh::RenUpdateDescriptorSetGBufferAttachment({
			.m_device = m_vkState().m_device,
			.m_gbufferImage = m_gBufferAttachments[ALBEDO],
			.m_binding = ALBEDO,
			.m_descriptorSet = m_descriptorSetComposition
			});

		vvh::RenUpdateDescriptorSetDepthAttachment({
			.m_device = m_vkState().m_device,
			.m_depthImage = m_vkState().m_depthImage,
			.m_binding = DEPTH,
			.m_descriptorSet = m_descriptorSetComposition,
			.m_sampler = m_sampler
			});
	}

	template<typename Derived>
	std::string RendererDeferredCommon<Derived>::getPipelineType(ObjectHandle handle, vvh::VertexData& vertexData) {
		std::string type = vertexData.getType();
		if (m_registry.template Has<TextureHandle>(handle) && type.find("U") != std::string::npos) type += "E";
		if (m_registry.template Has<vvh::Color>(handle) && type.find("C") == std::string::npos && type.find("E") == std::string::npos) type += "O";
		return type;
	}

	template<typename Derived>
	RendererDeferredCommon<Derived>::PipelinePerType* RendererDeferredCommon<Derived>::getPipelinePerType(std::string type) {
		for (auto& [pri, pipeline] : m_geomPipesPerType) {
			bool found = true;
			for (auto& c : pipeline.m_type) { found = found && (type.find(c) != std::string::npos); }
			if (found) return &pipeline;
		}
		std::cout << "Pipeline not found for type: " << type << std::endl;
		exit(-1);
		return nullptr;
	}

}	// namespace vve
