#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred13::RendererDeferred13(std::string systemName, Engine& engine, std::string windowName)
		: Renderer(systemName, engine, windowName) {

		engine.RegisterCallbacks({
			{this,  3500, "INIT",				[this](Message& message) { return OnInit(message); } },
			{this,  2000, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); } },
			{this,  2000, "RECORD_NEXT_FRAME",	[this](Message& message) { return OnRecordNextFrame(message); } },
			{this,  2000, "OBJECT_CREATE",		[this](Message& message) { return OnObjectCreate(message); } },
			{this, 10000, "OBJECT_DESTROY",		[this](Message& message) { return OnObjectDestroy(message); } },
			//{this,  1500, "WINDOW_SIZE",		[this](Message& message) { return OnWindowSize(message); }},
			{this, 	   0, "QUIT",				[this](Message& message) { return OnQuit(message); } }
			});
	}

	RendererDeferred13::~RendererDeferred13() {};

	bool RendererDeferred13::OnInit(Message message) {
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

		// TODO: rewrite into shared base class
		using GBufIndex = vve::RendererDeferred::GBufferIndex;

		vvh::RenCreateGBufferResources({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_swapChain = m_vkState().m_swapChain,
			.m_gbufferImage = m_gBufferAttachments[GBufIndex::POSITION],
			.m_format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.m_sampler = m_sampler
			});
		vvh::RenUpdateDescriptorSetGBufferAttachment({
			.m_device = m_vkState().m_device,
			.m_gbufferImage = m_gBufferAttachments[GBufIndex::POSITION],
			.m_binding = GBufIndex::POSITION,
			.m_descriptorSet = m_descriptorSetComposition
			});
		vvh::RenCreateGBufferResources({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_swapChain = m_vkState().m_swapChain,
			.m_gbufferImage = m_gBufferAttachments[GBufIndex::NORMAL],
			.m_format = VK_FORMAT_R8G8B8A8_UNORM,
			.m_sampler = m_sampler
			});
		vvh::RenUpdateDescriptorSetGBufferAttachment({
			.m_device = m_vkState().m_device,
			.m_gbufferImage = m_gBufferAttachments[GBufIndex::NORMAL],
			.m_binding = GBufIndex::NORMAL,
			.m_descriptorSet = m_descriptorSetComposition
			});
		vvh::RenCreateGBufferResources({
			.m_physicalDevice = m_vkState().m_physicalDevice,
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_swapChain = m_vkState().m_swapChain,
			.m_gbufferImage = m_gBufferAttachments[GBufIndex::ALBEDO],
			.m_format = VK_FORMAT_R8G8B8A8_SRGB,
			.m_sampler = m_sampler
			});
		vvh::RenUpdateDescriptorSetGBufferAttachment({
			.m_device = m_vkState().m_device,
			.m_gbufferImage = m_gBufferAttachments[GBufIndex::ALBEDO],
			.m_binding = GBufIndex::ALBEDO,
			.m_descriptorSet = m_descriptorSetComposition
			});

		vvh::RenUpdateDescriptorSetDepthAttachment({
			.m_device = m_vkState().m_device,
			.m_depthImage = m_vkState().m_depthImage,
			.m_binding = GBufIndex::DEPTH,
			.m_descriptorSet = m_descriptorSetComposition,
			.m_sampler = m_sampler
			});

		// GBuffer attachments from  VK_IMAGE_LAYOUT_UNDEFINED --> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		for (auto& image : m_gBufferAttachments) {
			vvh::ImgTransitionImageLayout3({
				.m_device = m_vkState().m_device,
				.m_graphicsQueue = m_vkState().m_graphicsQueue,
				.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
				.m_image = image.m_gbufferImage,
				.m_format = image.m_gbufferFormat,
				.m_oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.m_newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.m_commandBuffer = VK_NULL_HANDLE
				});
		}

		CreateGeometryPipeline();
		CreateLightingPipeline();

		return false;
	}

	bool RendererDeferred13::OnPrepareNextFrame(Message message) {
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

		return false;
	}

	bool RendererDeferred13::OnObjectCreate(Message message) {
		ObjectHandle oHandle = message.template GetData<MsgObjectCreate>().m_object;
		assert(m_registry.template Has<MeshHandle>(oHandle));
		auto meshHandle = m_registry.template Get<MeshHandle>(oHandle);
		auto mesh = m_registry.template Get<vvh::Mesh&>(meshHandle);
		auto type = getPipelineType(oHandle, mesh().m_verticesData);
		auto pipelinePerType = getPipelinePerType(type);

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
			auto tHandle = m_registry.template Get<TextureHandle>(oHandle);
			vvh::Image& texture = m_registry.template Get<vvh::Image&>(tHandle);
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
		return false;
	}

	bool RendererDeferred13::OnObjectDestroy(Message message) {
		auto& msg = message.template GetData<MsgObjectDestroy>();
		auto& oHandle = msg.m_handle();

		assert(m_registry.Exists(oHandle));

		if (!m_registry.template Has<vvh::Buffer>(oHandle)) return false;
		vvh::Buffer& ubo = m_registry.template Get<vvh::Buffer&>(oHandle);
		vvh::BufDestroyBuffer2({
			.m_device = m_vkState().m_device,
			.m_vmaAllocator = m_vkState().m_vmaAllocator,
			.m_buffers = ubo
			});
		return false;
	}

	bool RendererDeferred13::OnRecordNextFrame(Message message) {

		std::vector<VkCommandBuffer> cmdBuffers(1);
		vvh::ComCreateCommandBuffers({
			.m_device = m_vkState().m_device,
			.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
			.m_commandBuffers = cmdBuffers
			});
		auto cmdBuffer = cmdBuffers[0];

		std::vector<VkClearValue> clearValues(4);
		glm::vec4 clearColor{ 0,0,0,1 };

		clearValues[0].color = { {clearColor.r, clearColor.g, clearColor.b, clearColor.w} };
		clearValues[1].color = { {clearColor.r, clearColor.g, clearColor.b, clearColor.w} };
		clearValues[2].color = { {clearColor.r, clearColor.g, clearColor.b, clearColor.w} };
		clearValues[3].depthStencil = { 1.0f, 0 };

		vvh::ComBeginCommandBuffer({ cmdBuffer });

		// TODO: Maybe add KHR way for extension support?
		// Infos were in render pass previously
		VkRenderingAttachmentInfo gbufferAttach[3] = {};
		size_t i = 0;
		for (const auto& attach : m_gBufferAttachments) {
			gbufferAttach[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			gbufferAttach[i].pNext = VK_NULL_HANDLE;
			gbufferAttach[i].imageView = attach.m_gbufferImageView;
			gbufferAttach[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			gbufferAttach[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			gbufferAttach[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			gbufferAttach[i].clearValue = clearValues[i];
			i++;
		}

		VkRenderingAttachmentInfo depthAttach = {};
		depthAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttach.pNext = VK_NULL_HANDLE;
		depthAttach.imageView = m_vkState().m_depthImage.m_depthImageView;
		depthAttach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttach.clearValue = clearValues[3];

		auto renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent};
		VkRenderingInfo geometryRenderingInfo = {};
		geometryRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		geometryRenderingInfo.pNext = VK_NULL_HANDLE;
		geometryRenderingInfo.flags = 0;
		geometryRenderingInfo.renderArea = renderArea;
		geometryRenderingInfo.layerCount = 1;
		geometryRenderingInfo.viewMask = 0;
		geometryRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(m_gBufferAttachments.size());
		geometryRenderingInfo.pColorAttachments = gbufferAttach;
		geometryRenderingInfo.pDepthAttachment = &depthAttach;
		geometryRenderingInfo.pStencilAttachment = nullptr;

		// new call
		vkCmdBeginRendering(cmdBuffer, &geometryRenderingInfo);


		float f = 0.0;
		std::array<float, 4> blendconst = (m_pass == 0 ? std::array<float, 4>{f, f, f, f} : std::array<float, 4>{ 1 - f,1 - f,1 - f,1 - f });

		for (auto& pipeline : m_geomPipesPerType) {

			vvh::Pipeline pip{
				pipeline.second.m_graphicsPipeline.m_pipelineLayout,
				pipeline.second.m_graphicsPipeline.m_pipeline
			};

			vvh::ComBindPipeline({
				.m_commandBuffer = cmdBuffer,
				.m_graphicsPipeline = pip,
				.m_imageIndex = m_vkState().m_imageIndex,
				.m_swapChain = m_vkState().m_swapChain,
				.m_renderPass = VK_NULL_HANDLE,
				.m_viewPorts = {},
				.m_scissors = {}, //default view ports and scissors
				.m_blendConstants = blendconst,
				.m_pushConstants = {},
				.m_currentFrame = m_vkState().m_currentFrame
				});

			for (auto [oHandle, name, ghandle, LtoW, uniformBuffers, descriptorsets] :
				m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vvh::Buffer&, vvh::DescriptorSet&>
				({ (size_t)pipeline.second.m_graphicsPipeline.m_pipeline })) {

				bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
				bool hasColor = m_registry.template Has<vvh::Color>(oHandle);
				bool hasVertexColor = pipeline.second.m_type.find("C") != std::string::npos;
				if (!hasTexture && !hasColor && !hasVertexColor) continue;

				vvh::Mesh& mesh = m_registry.template Get<vvh::Mesh&>(ghandle);
				vvh::ComRecordObject({
					.m_commandBuffer = cmdBuffer,
					.m_graphicsPipeline = pipeline.second.m_graphicsPipeline,
					.m_descriptorSets = { m_descriptorSetPerFrame, descriptorsets },
					.m_type = pipeline.second.m_type,
					.m_mesh = mesh,
					.m_currentFrame = m_vkState().m_currentFrame
					});
			}
		}
		//vvh::ComEndRenderPass({ .m_commandBuffer = cmdBuffer });
		vkCmdEndRendering(cmdBuffer);

		// ---------------------------------------------------------------------
		// Lighting pass

		// GBuffer attachments VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL --> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		for (auto& image : m_gBufferAttachments) {
			vvh::ImgTransitionImageLayout3({
				.m_device = m_vkState().m_device,
				.m_graphicsQueue = m_vkState().m_graphicsQueue,
				.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
				.m_image = image.m_gbufferImage,
				.m_format = image.m_gbufferFormat,
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
				.m_format = vvh::RenFindDepthFormat(m_vkState().m_physicalDevice),
				.m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
				.m_oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.m_newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.m_commandBuffer = cmdBuffer
			});

		VkRenderingAttachmentInfo outAttach = {};
		outAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		outAttach.pNext = VK_NULL_HANDLE;
		outAttach.imageView = m_vkState().m_swapChain.m_swapChainImageViews[m_vkState().m_imageIndex];
		outAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		outAttach.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		outAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		VkRenderingInfo lightingRenderingInfo = {};
		lightingRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		lightingRenderingInfo.pNext = VK_NULL_HANDLE;
		lightingRenderingInfo.flags = 0;
		lightingRenderingInfo.renderArea = renderArea;
		lightingRenderingInfo.layerCount = 1;
		lightingRenderingInfo.viewMask = 0;
		lightingRenderingInfo.colorAttachmentCount = 1;
		lightingRenderingInfo.pColorAttachments = &outAttach;
		lightingRenderingInfo.pDepthAttachment = nullptr;
		lightingRenderingInfo.pStencilAttachment = nullptr;

		vkCmdBeginRendering(cmdBuffer, &lightingRenderingInfo);

		vvh::LightOffset offset{ 0, m_numberLightsPerType.x + m_numberLightsPerType.y + m_numberLightsPerType.z };

		vvh::ComBindPipeline({
			.m_commandBuffer = cmdBuffer,
			.m_graphicsPipeline = m_lightingPipeline,
			.m_imageIndex = m_vkState().m_imageIndex,
			.m_swapChain = m_vkState().m_swapChain,
			.m_renderPass = VK_NULL_HANDLE,
			.m_viewPorts = {},
			.m_scissors = {}, //default view ports and scissors
			.m_blendConstants = blendconst,
			.m_pushConstants = {
				{
					.layout = m_lightingPipeline.m_pipelineLayout,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.offset = 0,
					.size = sizeof(offset),
					.pValues = &offset
				}
			},
			.m_currentFrame = m_vkState().m_currentFrame
			});

		VkDescriptorSet sets[] = { m_descriptorSetPerFrame.m_descriptorSetPerFrameInFlight[m_vkState().m_currentFrame],
			m_descriptorSetComposition.m_descriptorSetPerFrameInFlight[m_vkState().m_currentFrame] };

		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline.m_pipelineLayout,
			0, 2, sets, 0, nullptr);

		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

		//vvh::ComEndRenderPass({ .m_commandBuffer = cmdBuffer });
		vkCmdEndRendering(cmdBuffer);

		// GBuffer attachments from  VK_IMAGE_LAYOUT_UNDEFINED --> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		for (auto& image : m_gBufferAttachments) {
			vvh::ImgTransitionImageLayout3({
				.m_device = m_vkState().m_device,
				.m_graphicsQueue = m_vkState().m_graphicsQueue,
				.m_commandPool = m_commandPools[m_vkState().m_currentFrame],
				.m_image = image.m_gbufferImage,
				.m_format = image.m_gbufferFormat,
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
			.m_format = vvh::RenFindDepthFormat(m_vkState().m_physicalDevice),
			.m_aspect = VK_IMAGE_ASPECT_DEPTH_BIT,
			.m_oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.m_newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.m_commandBuffer = cmdBuffer
			});

		vvh::ComEndCommandBuffer({ .m_commandBuffer = cmdBuffer });
		SubmitCommandBuffer(cmdBuffer);

		++m_pass;
		return false;
	}

	bool RendererDeferred13::OnQuit(Message message) {
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

		for (auto& buffer : { std::ref(m_uniformBuffersPerFrame), std::ref(m_storageBuffersLights) }) {
			vvh::BufDestroyBuffer2({
				.m_device = m_vkState().m_device,
				.m_vmaAllocator = m_vkState().m_vmaAllocator,
				.m_buffers = buffer.get()
				});
		}

		for (auto& gBufferAttach : m_gBufferAttachments) {
			vvh::ImgDestroyImage({
				.m_device = m_vkState().m_device,
				.m_vmaAllocator = m_vkState().m_vmaAllocator,
				.m_image = gBufferAttach.m_gbufferImage,
				.m_imageAllocation = gBufferAttach.m_gbufferImageAllocation
				});
			vkDestroyImageView(m_vkState().m_device, gBufferAttach.m_gbufferImageView, nullptr);
		}

		return false;
	}

	auto RendererDeferred13::getAttachmentFormats() -> std::vector<VkFormat> {
		std::vector<VkFormat> attachFormats;
		attachFormats.reserve(m_gBufferAttachments.size());
		for (const auto& attach : m_gBufferAttachments) {
			attachFormats.emplace_back(attach.m_gbufferFormat);
		}

		return attachFormats;
	}

	void RendererDeferred13::CreateGeometryPipeline() {
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
				// TODO: colorBlendAttachment.colorWriteMask = 0xf; ???
				// TODO: rewrite to make use for the 3 attachments clearer
				colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
				colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
				colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
				colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
				colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
				colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
				colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_MAX;
				colorBlendAttachment.blendEnable = VK_TRUE;

				vvh::RenCreateGraphicsPipelineDynamic({
					.m_device = m_vkState().m_device,
					.m_vertShaderPath = entry.path().string(),
					.m_fragShaderPath = entry.path().string(),
					.m_bindingDescription = bindingDescriptions,
					.m_attributeDescriptions = attributeDescriptions,
					.m_descriptorSetLayouts = { m_descriptorSetLayoutPerFrame, descriptorSetLayoutPerObject },
					.m_specializationConstants = {},
					.m_pushConstantRanges = {},
					.m_blendAttachments = { colorBlendAttachment, colorBlendAttachment, colorBlendAttachment },
					.m_graphicsPipeline = graphicsPipeline,
					.m_attachmentFormats = getAttachmentFormats(),
					.m_depthFormat = vvh::RenFindDepthFormat(m_vkState().m_physicalDevice),
					.m_depthWrite = true
					});

				m_geomPipesPerType[pri] = { type, descriptorSetLayoutPerObject, graphicsPipeline };
			}
		}
	}

	void RendererDeferred13::CreateLightingPipeline() {
		const std::filesystem::path shaders{ "shaders/Deferred" };
		if (!std::filesystem::exists(shaders)) {
			std::cerr << "ERROR: Folder does not exist: " << std::filesystem::absolute(shaders) << "\n";
		}
		const std::string vert = (shaders / "test_lighting.spv").string();
		const std::string frag = (shaders / "test_lighting.spv").string();

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		// TODO: colorBlendAttachment.colorWriteMask = 0xf; ???
		// TODO: rewrite to make use for the 3 attachments clearer
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_MAX;
		colorBlendAttachment.blendEnable = VK_TRUE;

		vvh::RenCreateGraphicsPipelineDynamic({
			.m_device = m_vkState().m_device,
			.m_vertShaderPath = vert,
			.m_fragShaderPath = frag,
			.m_bindingDescription = {},
			.m_attributeDescriptions = {},
			.m_descriptorSetLayouts = { m_descriptorSetLayoutPerFrame, m_descriptorSetLayoutComposition },
			.m_specializationConstants = { MAX_NUMBER_LIGHTS },
			.m_pushConstantRanges = { {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = 8} },
			.m_blendAttachments = { colorBlendAttachment },
			.m_graphicsPipeline = m_lightingPipeline,
			.m_attachmentFormats = { m_vkState().m_swapChain.m_swapChainImageFormat },
			.m_depthFormat = vvh::RenFindDepthFormat(m_vkState().m_physicalDevice),
			.m_depthWrite = false
			});
	}

	std::string RendererDeferred13::getPipelineType(ObjectHandle handle, vvh::VertexData& vertexData) {
		std::string type = vertexData.getType();
		if (m_registry.template Has<TextureHandle>(handle) && type.find("U") != std::string::npos) type += "E";
		if (m_registry.template Has<vvh::Color>(handle) && type.find("C") == std::string::npos && type.find("E") == std::string::npos) type += "O";
		return type;
	}

	RendererDeferred13::PipelinePerType* RendererDeferred13::getPipelinePerType(std::string type) {
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
