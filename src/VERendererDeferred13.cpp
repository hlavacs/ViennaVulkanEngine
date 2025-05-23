#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred13::RendererDeferred13(std::string systemName, Engine& engine, std::string windowName)
		: Renderer(systemName, engine, windowName) {

		engine.RegisterCallbacks({
			{this,  3500, "INIT",				[this](Message& message) { return OnInit(message); } },
			//{this,  2000, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); } },
			//{this,  2000, "RECORD_NEXT_FRAME",	[this](Message& message) { return OnRecordNextFrame(message); } },
			//{this,  2000, "OBJECT_CREATE",		[this](Message& message) { return OnObjectCreate(message); } },
			//{this, 10000, "OBJECT_DESTROY",		[this](Message& message) { return OnObjectDestroy(message); } },
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

		CreateGeometryPipeline();
		CreateLightingPipeline();

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

}	// namespace vve
