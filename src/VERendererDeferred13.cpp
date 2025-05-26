#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred13::RendererDeferred13(std::string systemName, Engine& engine, std::string windowName)
		: RendererDeferredCommon(systemName, engine, windowName) {}

	RendererDeferred13::~RendererDeferred13() {};

	bool RendererDeferred13::OnInit(Message message) {

		CreateGeometryPipeline();
		CreateLightingPipeline();

		return false;
	}

	bool RendererDeferred13::OnPrepareNextFrame(Message message) {
		// no extra steps aside from base class
		return false;
	}

	bool RendererDeferred13::OnObjectCreate(Message message) {
		// empty
		return false;
	}

	bool RendererDeferred13::OnObjectDestroy(Message message) {
		// empty
		return false;
	}

	bool RendererDeferred13::OnRecordNextFrame(Message message) {

		auto cmdBuffer = m_commandBuffers[m_vkState().m_currentFrame];
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
			gbufferAttach[i].clearValue = m_clearValues[i];
			i++;
		}

		VkRenderingAttachmentInfo depthAttach = {};
		depthAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttach.pNext = VK_NULL_HANDLE;
		depthAttach.imageView = m_vkState().m_depthImage.m_depthImageView;
		depthAttach.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depthAttach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttach.clearValue = m_clearValues[DEPTH];

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
				.m_format = m_vkState().m_depthMapFormat,
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
			.m_format = m_vkState().m_depthMapFormat,
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

	bool RendererDeferred13::OnWindowSize(Message message) {
		// empty
		return false;
	}

	bool RendererDeferred13::OnQuit(Message message) {
		// empty
		return false;
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
