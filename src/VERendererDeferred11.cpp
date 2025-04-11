#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred11::RendererDeferred11(std::string systemName, Engine& engine, std::string windowName)
		: Renderer(systemName, engine, windowName) {

		engine.RegisterCallbacks({
			{this, 3500, "INIT", [this](Message& message) { return OnInit(message); } },
			{this, 2000, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); } },
			{this, 2000, "RECORD_NEXT_FRAME", [this](Message& message) { return OnRecordNextFrame(message); } },
			{this,  000, "OBJECT_CREATE", [this](Message& message) { return OnObjectCreate(message); } },
			{this,10000, "OBJECT_DESTROY", [this](Message& message) { return OnObjectDestroy(message); } },
			{this,	  0, "QUIT", [this](Message& message) { return OnQuit(message); } }
			});
	}

	RendererDeferred11::~RendererDeferred11() {};

	bool RendererDeferred11::OnInit(Message message) {
		// TODO: maybe a reference will be enough here to not make a message copy?
		Renderer::OnInit(message);

		vh::RenCreateRenderPassGeometry(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_swapChain, true, m_geometryPass);
		vh::RenCreateRenderPass(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_swapChain, true, m_lightingPass);

		// TODO: binding 0 might only need vertex globally
		vh::RenCreateDescriptorSetLayout(
			m_vkState().m_device,
			{
				{	// Binding 0 : Vertex and Fragment uniform buffer
					.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 1 : Position
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 2 : Normals
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 3 : Albedo
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
				{	// Binding 4 : Light uniform buffer
					.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
			},
			m_descriptorSetLayoutPerFrame);

		//vh::ComCreateCommandPool(m_vkState().m_surface, m_vkState().m_physicalDevice, m_vkState().m_device, m_commandPool);
		m_commandPools.resize(MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vh::ComCreateCommandPool(m_vkState().m_surface, m_vkState().m_physicalDevice, m_vkState().m_device, m_commandPools[i]);
		}

		// TODO: shrink pool to only what is needed - why 1000?
		vh::RenCreateDescriptorPool(m_vkState().m_device, 1000, m_descriptorPool);
		vh::RenCreateDescriptorSet(m_vkState().m_device, m_descriptorSetLayoutPerFrame, m_descriptorPool, m_descriptorSetComposition);

		// Binding 0 : Vertex and Fragment uniform buffer
		// TODO : Only needed for per Object? - research
		//vh::BufCreateBuffers(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(vh::UniformBufferFrame), m_uniformBuffersPerFrame);
		//vh::RenUpdateDescriptorSet(m_vkState().m_device, m_uniformBuffersPerFrame, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeof(vh::UniformBufferFrame), m_descriptorSetPerFrame);

		// Deferred Composition

		vh::ImgCreateImageSampler(m_vkState().m_physicalDevice, m_vkState().m_device, m_sampler);
		// Binding 1 : Position
		vh::RenCreateGBufferResources(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, m_vkState().m_swapChain, m_gBufferAttachments[POSITION], m_vkState().m_swapChain.m_swapChainImageFormat, m_sampler);
		vh::RenUpdateDescriptorSetGBufferAttachment(m_vkState().m_device, m_gBufferAttachments[POSITION], 1, m_descriptorSetComposition);
		// Binding 2 : Normals
		vh::RenCreateGBufferResources(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, m_vkState().m_swapChain, m_gBufferAttachments[NORMALS], m_vkState().m_swapChain.m_swapChainImageFormat, m_sampler);
		vh::RenUpdateDescriptorSetGBufferAttachment(m_vkState().m_device, m_gBufferAttachments[NORMALS], 2, m_descriptorSetComposition);
		// Binding 3 : Albedo
		vh::RenCreateGBufferResources(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, m_vkState().m_swapChain, m_gBufferAttachments[ALBEDO], VK_FORMAT_R8G8B8A8_UNORM, m_sampler);
		vh::RenUpdateDescriptorSetGBufferAttachment(m_vkState().m_device, m_gBufferAttachments[ALBEDO], 3, m_descriptorSetComposition);
		// Binding 4 : Light
		vh::BufCreateBuffers(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, MAX_NUMBER_LIGHTS * sizeof(vh::Light), m_uniformBuffersLights);
		vh::RenUpdateDescriptorSet(m_vkState().m_device, m_uniformBuffersLights, 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_NUMBER_LIGHTS * sizeof(vh::Light), m_descriptorSetComposition);

		// Object Descriptors
		
		// Binding 0 : Vertex and Fragment uniform buffer
		vh::BufCreateBuffers(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(vh::UniformBufferFrame), m_uniformBuffersPerFrame);
		vh::RenUpdateDescriptorSet(m_vkState().m_device, m_uniformBuffersPerFrame, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeof(vh::UniformBufferFrame), m_descriptorSetObject);
		// Binding 1 : Position
		vh::RenCreateGBufferResources(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, m_vkState().m_swapChain, m_gBufferAttachments[POSITION], m_vkState().m_swapChain.m_swapChainImageFormat, m_sampler);
		vh::RenUpdateDescriptorSetGBufferAttachment(m_vkState().m_device, m_gBufferAttachments[POSITION], 1, m_descriptorSetObject);
		// Binding 2 : Normals
		vh::RenCreateGBufferResources(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, m_vkState().m_swapChain, m_gBufferAttachments[NORMALS], m_vkState().m_swapChain.m_swapChainImageFormat, m_sampler);
		vh::RenUpdateDescriptorSetGBufferAttachment(m_vkState().m_device, m_gBufferAttachments[NORMALS], 2, m_descriptorSetObject);


		vh::RenCreateGBufferFramebuffers(m_vkState().m_device, m_vkState().m_swapChain, m_gBufferAttachments, m_gBufferFrameBuffers, m_vkState().m_depthImage, m_geometryPass);

		CreateGeometryPipeline();
		CreateLightingPipeline();
		return false;
	}

	bool RendererDeferred11::OnPrepareNextFrame(Message message) {
		vh::UniformBufferFrame ubc;
		vkResetCommandPool(m_vkState().m_device, m_commandPools[m_vkState().m_currentFrame], 0);

		int total{ 0 };
		std::vector<vh::Light> lights{ MAX_NUMBER_LIGHTS };
		m_numberLightsPerType.x = RegisterLight<PointLight>(1.0f, lights, total);
		m_numberLightsPerType.y = RegisterLight<DirectionalLight>(2.0f, lights, total);
		m_numberLightsPerType.z = RegisterLight<SpotLight>(3.0f, lights, total);
		ubc.numLights = m_numberLightsPerType;
		memcpy(m_uniformBuffersLights.m_uniformBuffersMapped[m_vkState().m_currentFrame], lights.data(), total * sizeof(vh::Light));

		auto [lToW, view, proj] = *m_registry.template GetView<LocalToWorldMatrix&, ViewMatrix&, ProjectionMatrix&>().begin();
		ubc.camera.view = view();
		ubc.camera.proj = proj();
		ubc.camera.positionW = lToW()[3];
		memcpy(m_uniformBuffersPerFrame.m_uniformBuffersMapped[m_vkState().m_currentFrame], &ubc, sizeof(ubc));

		for (auto [oHandle, name, ghandle, LtoW, uniformBuffers] :
			m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vh::Buffer&>({ (size_t)m_geometryPipeline.m_pipeline })) {

			bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
			bool hasColor = m_registry.template Has<vh::Color>(oHandle);
			bool hasVertexColor = true;		//pipeline.second.m_type.find("C") != std::string::npos;
			if (!hasTexture && !hasColor && !hasVertexColor) continue;

			if (hasTexture) {
				vh::BufferPerObjectTexture uboTexture{};
				uboTexture.model = LtoW();
				uboTexture.modelInverseTranspose = glm::inverse(glm::transpose(uboTexture.model));
				UVScale uvScale{ { 1.0f, 1.0f } };
				if (m_registry.template Has<UVScale>(oHandle)) { uvScale = m_registry.template Get<UVScale>(oHandle); }
				uboTexture.uvScale = uvScale;
				memcpy(uniformBuffers().m_uniformBuffersMapped[m_vkState().m_currentFrame], &uboTexture, sizeof(uboTexture));
			}
			else if (hasColor) {
				vh::BufferPerObjectColor uboColor{};
				uboColor.model = LtoW();
				uboColor.modelInverseTranspose = glm::inverse(glm::transpose(uboColor.model));
				uboColor.color = m_registry.template Get<vh::Color>(oHandle);
				memcpy(uniformBuffers().m_uniformBuffersMapped[m_vkState().m_currentFrame], &uboColor, sizeof(uboColor));
			}
			else if (hasVertexColor) {
				vh::BufferPerObject uboColor{};
				uboColor.model = LtoW();
				uboColor.modelInverseTranspose = glm::inverse(glm::transpose(uboColor.model));
				memcpy(uniformBuffers().m_uniformBuffersMapped[m_vkState().m_currentFrame], &uboColor, sizeof(uboColor));
			}
		}

		return false;
	}

	bool RendererDeferred11::OnRecordNextFrame(Message message) {

		std::vector<VkCommandBuffer> cmdBuffers(1);
		vh::ComCreateCommandBuffers(m_vkState().m_device, m_commandPools[m_vkState().m_currentFrame], cmdBuffers);
		auto cmdBuffer = cmdBuffers[0];

		std::vector<VkClearValue> clearValues(4);
		glm::vec4 clearColor{};

		clearValues[0].color = { {clearColor.r, clearColor.g, clearColor.b, clearColor.w} };
		clearValues[1].color = { {clearColor.r, clearColor.g, clearColor.b, clearColor.w} };
		clearValues[2].color = { {clearColor.r, clearColor.g, clearColor.b, clearColor.w} };
		clearValues[3].depthStencil = { 1.0f, 0 };

		vh::ComStartRecordCommandBufferClearValue(cmdBuffer, m_vkState().m_imageIndex,
			m_vkState().m_swapChain, m_gBufferFrameBuffers,
			m_geometryPass, clearValues,
			m_vkState().m_currentFrame);

		float f = 0.0;
		std::array<float, 4> blendconst = (0 == 0 ? std::array<float, 4>{f, f, f, f} : std::array<float, 4>{ 1 - f,1 - f,1 - f,1 - f });

		vh::LightOffset offset{ 0, m_numberLightsPerType.x + m_numberLightsPerType.y + m_numberLightsPerType.z };
		vh::ComBindPipeline(
			cmdBuffer,
			m_vkState().m_imageIndex,
			m_vkState().m_swapChain,
			m_geometryPass,
			m_geometryPipeline,
			{}, {}, //default view ports and scissors
			blendconst, //blend constants
			{
				{.layout = m_geometryPipeline.m_pipelineLayout,
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
					.offset = 0,
					.size = sizeof(offset),
					.pValues = &offset
				}
			}, //push constants
			m_vkState().m_currentFrame);

		for (auto [oHandle, name, ghandle, LtoW, uniformBuffers, descriptorsets] :
			m_registry.template GetView<vecs::Handle, Name, MeshHandle, LocalToWorldMatrix&, vh::Buffer&, vh::DescriptorSet&>
			({ (size_t)m_geometryPipeline.m_pipeline })) {

			bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
			bool hasColor = m_registry.template Has<vh::Color>(oHandle);
			bool hasVertexColor = true;		// pipeline.second.m_type.find("C") != std::string::npos;
			if (!hasTexture && !hasColor && !hasVertexColor) continue;

			auto mesh = m_registry.template Get<vh::Mesh&>(ghandle);
			vh::ComRecordObject(cmdBuffer, m_geometryPipeline,
				{ m_descriptorSetComposition, descriptorsets }, "C", mesh, m_vkState().m_currentFrame);
		}

		vh::ComEndRecordCommandBuffer(cmdBuffer);
		SubmitCommandBuffer(cmdBuffer);

		//++m_pass;
		return false;
	}

	bool RendererDeferred11::OnObjectCreate(Message message) {
		ObjectHandle oHandle = message.template GetData<MsgObjectCreate>().m_object;
		assert(m_registry.template Has<MeshHandle>(oHandle));
		auto meshHandle = m_registry.template Get<MeshHandle>(oHandle);
		auto mesh = m_registry.template Get<vh::Mesh&>(meshHandle);
		auto type = "C"; // getPipelineType(oHandle, mesh().m_verticesData);
		auto pipelinePerType = "C"; // getPipelinePerType(type);

		bool hasTexture = m_registry.template Has<TextureHandle>(oHandle);
		bool hasColor = m_registry.template Has<vh::Color>(oHandle);
		bool hasVertexColor = true; // pipelinePerType->m_type.find("C") != std::string::npos;
		if (!hasTexture && !hasColor && !hasVertexColor) return false;

		vh::Buffer ubo;
		size_t sizeUbo = 0;
		vh::DescriptorSet descriptorSet{ 1 };
		vh::RenCreateDescriptorSet(m_vkState().m_device,m_descriptorSetLayoutPerFrame, m_descriptorPool, descriptorSet);

		if (hasTexture) {
			sizeUbo = sizeof(vh::BufferPerObjectTexture);
			auto tHandle = m_registry.template Get<TextureHandle>(oHandle);
			auto texture = m_registry.template Get<vh::Map&>(tHandle);
			vh::RenUpdateDescriptorSetTexture(m_vkState().m_device, texture, 1, descriptorSet);
		}
		else if (hasColor) {
			sizeUbo = sizeof(vh::BufferPerObjectColor);
		}
		else if (hasVertexColor) {
			sizeUbo = sizeof(vh::BufferPerObject);
		}

		vh::BufCreateBuffers(m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeUbo, ubo);
		vh::RenUpdateDescriptorSet(m_vkState().m_device, ubo, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizeUbo, descriptorSet);

		m_registry.Put(oHandle, ubo, descriptorSet);
		m_registry.AddTags(oHandle, (size_t)m_geometryPipeline.m_pipeline);

		assert(m_registry.template Has<vh::Buffer>(oHandle));
		assert(m_registry.template Has<vh::DescriptorSet>(oHandle));
		return false; //true if handled
	}

	bool RendererDeferred11::OnObjectDestroy(Message message) {
		auto& msg = message.template GetData<MsgObjectDestroy>();
		auto& oHandle = msg.m_handle();

		assert(m_registry.Exists(oHandle));

		if (!m_registry.template Has<vh::Buffer>(oHandle)) return false;
		auto ubo = m_registry.template Get<vh::Buffer&>(oHandle);
		vh::BufDestroyBuffer2(m_vkState().m_device, m_vkState().m_vmaAllocator, ubo);
		return false;
	}

	bool RendererDeferred11::OnQuit(Message message) {
		vkDeviceWaitIdle(m_vkState().m_device);

		for (auto pool : m_commandPools) {
			vkDestroyCommandPool(m_vkState().m_device, pool, nullptr);
		}

		// TODO: Manage pipelines - rewrite into functions most likely
		vkDestroyPipeline(m_vkState().m_device, m_geometryPipeline.m_pipeline, nullptr);
		vkDestroyPipelineLayout(m_vkState().m_device, m_geometryPipeline.m_pipelineLayout, nullptr);
		vkDestroyPipeline(m_vkState().m_device, m_lightingPipeline.m_pipeline, nullptr);
		vkDestroyPipelineLayout(m_vkState().m_device, m_lightingPipeline.m_pipelineLayout, nullptr);

		vkDestroyDescriptorPool(m_vkState().m_device, m_descriptorPool, nullptr);
		vkDestroyRenderPass(m_vkState().m_device, m_geometryPass, nullptr);
		vkDestroyRenderPass(m_vkState().m_device, m_lightingPass, nullptr);
		vkDestroySampler(m_vkState().m_device, m_sampler, nullptr);

		vh::BufDestroyBuffer2(m_vkState().m_device, m_vkState().m_vmaAllocator, m_uniformBuffersPerFrame);
		vh::ImgDestroyImage(m_vkState().m_device, m_vkState().m_vmaAllocator, m_gBufferAttachments[POSITION].m_gbufferImage, m_gBufferAttachments[POSITION].m_gbufferImageAllocation);
		vh::ImgDestroyImage(m_vkState().m_device, m_vkState().m_vmaAllocator, m_gBufferAttachments[NORMALS].m_gbufferImage, m_gBufferAttachments[NORMALS].m_gbufferImageAllocation);
		vh::ImgDestroyImage(m_vkState().m_device, m_vkState().m_vmaAllocator, m_gBufferAttachments[ALBEDO].m_gbufferImage, m_gBufferAttachments[ALBEDO].m_gbufferImageAllocation);
		vkDestroyImageView(m_vkState().m_device, m_gBufferAttachments[POSITION].m_gbufferImageView, nullptr);
		vkDestroyImageView(m_vkState().m_device, m_gBufferAttachments[NORMALS].m_gbufferImageView, nullptr);
		vkDestroyImageView(m_vkState().m_device, m_gBufferAttachments[ALBEDO].m_gbufferImageView, nullptr);
		vh::BufDestroyBuffer2(m_vkState().m_device, m_vkState().m_vmaAllocator, m_uniformBuffersLights);

		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerFrame, nullptr);

		return false;
	}

	void RendererDeferred11::CreateGeometryPipeline() {
		const std::filesystem::path shaders{ "../../shaders/Deferred" };
		if (!std::filesystem::exists(shaders)) {
			std::cerr << "ERROR: Folder does not exist: " << std::filesystem::absolute(shaders) << "\n";
		}
		const std::string vert = (shaders / "test_geometry.spv").string();
		const std::string frag = (shaders / "test_geometry.spv").string();
		std::vector<VkVertexInputBindingDescription> bindingDescriptions = getBindingDescriptions();
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions = getAttributeDescriptions();

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		// TODO: colorBlendAttachment.colorWriteMask = 0xf; ???
		// TODO: rewrite to make use for the 3 attachments clearer
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_ALPHA;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_MAX;
		colorBlendAttachment.blendEnable = VK_FALSE;

		vh::RenCreateGraphicsPipeline(m_vkState().m_device, m_geometryPass, vert, frag, bindingDescriptions, attributeDescriptions,
			{ m_descriptorSetLayoutPerFrame }, { MAX_NUMBER_LIGHTS },
			{ {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = 8} },
			{ colorBlendAttachment, colorBlendAttachment, colorBlendAttachment }, m_geometryPipeline, true);
	}

	void RendererDeferred11::CreateLightingPipeline() {
		const std::filesystem::path shaders{ "../../shaders/Deferred" };
		if (!std::filesystem::exists(shaders)) {
			std::cerr << "ERROR: Folder does not exist: " << std::filesystem::absolute(shaders) << "\n";
		}
		const std::string vert = (shaders / "test_lighting.spv").string();
		const std::string frag = (shaders / "test_lighting.spv").string();

		vh::RenCreateGraphicsPipeline(m_vkState().m_device, m_lightingPass, vert, frag, {}, {},
			{ m_descriptorSetLayoutPerFrame }, { MAX_NUMBER_LIGHTS },
			{ {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = 8} },
			{}, m_lightingPipeline, true);
	}

	void RendererDeferred11::getBindingDescription(int binding, int stride, auto& bdesc) {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = binding;
		bindingDescription.stride = stride;
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bdesc.push_back(bindingDescription);
	}

	auto RendererDeferred11::getBindingDescriptions() -> std::vector<VkVertexInputBindingDescription> {
		std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
		int binding = 0;
		getBindingDescription(binding++, vh::VertexData::size_pos, bindingDescriptions);
		getBindingDescription(binding++, vh::VertexData::size_nor, bindingDescriptions);
		getBindingDescription(binding++, vh::VertexData::size_tex, bindingDescriptions);

		return bindingDescriptions;
	}

	void RendererDeferred11::getAttributeDescription(int binding, int location, VkFormat format, auto& attd) {
		VkVertexInputAttributeDescription attributeDescription{};
		attributeDescription.binding = binding;
		attributeDescription.location = location;
		attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescription.offset = 0;
		attd.push_back(attributeDescription);
	}

	auto RendererDeferred11::getAttributeDescriptions() -> std::vector<VkVertexInputAttributeDescription> {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
		int binding = 0;
		int location = 0;
		getAttributeDescription(binding++, location++, m_gBufferAttachments[POSITION].m_gbufferFormat, attributeDescriptions);
		getAttributeDescription(binding++, location++, m_gBufferAttachments[NORMALS].m_gbufferFormat, attributeDescriptions);
		getAttributeDescription(binding++, location++, m_gBufferAttachments[ALBEDO].m_gbufferFormat, attributeDescriptions);

		return attributeDescriptions;
	}

	template<typename T>
	auto RendererDeferred11::RegisterLight(float type, std::vector<vh::Light>& lights, int& total) -> int {
		int n = 0;
		for (auto [handle, light, lToW] : m_registry.template GetView<vecs::Handle, T&, LocalToWorldMatrix&>()) {
			++n;
			light().params.x = type;
			lights[total] = { .positionW = glm::vec3{lToW()[3]}, .directionW = glm::vec3{lToW()[1]}, .lightParams = light() };
			if (++total >= MAX_NUMBER_LIGHTS) return n;
		};
		return n;
	}

}	// namespace vve
