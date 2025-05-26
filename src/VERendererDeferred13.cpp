#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred13::RendererDeferred13(std::string systemName, Engine& engine, std::string windowName)
		: RendererDeferredCommon(systemName, engine, windowName) {}

	RendererDeferred13::~RendererDeferred13() {};

	bool RendererDeferred13::OnInit(Message message) {

		// TODO: Maybe add KHR way for extension support?
		// geometry
		size_t i = 0;
		for (const auto& attach : m_gBufferAttachments) {
			m_gbufferRenderingInfo[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			m_gbufferRenderingInfo[i].pNext = VK_NULL_HANDLE;
			m_gbufferRenderingInfo[i].imageView = attach.m_gbufferImageView;
			m_gbufferRenderingInfo[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			m_gbufferRenderingInfo[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			m_gbufferRenderingInfo[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			m_gbufferRenderingInfo[i].clearValue = m_clearValues[i];
			i++;
		}

		m_depthRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		m_depthRenderingInfo.pNext = VK_NULL_HANDLE;
		m_depthRenderingInfo.imageView = m_vkState().m_depthImage.m_depthImageView;
		m_depthRenderingInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		m_depthRenderingInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		m_depthRenderingInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		m_depthRenderingInfo.clearValue = m_clearValues[DEPTH];

		VkRect2D renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent };
		m_geometryRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		m_geometryRenderingInfo.pNext = VK_NULL_HANDLE;
		m_geometryRenderingInfo.flags = 0;
		m_geometryRenderingInfo.renderArea = renderArea;
		m_geometryRenderingInfo.layerCount = 1;
		m_geometryRenderingInfo.viewMask = 0;
		m_geometryRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(m_gBufferAttachments.size());
		m_geometryRenderingInfo.pColorAttachments = m_gbufferRenderingInfo;
		m_geometryRenderingInfo.pDepthAttachment = &m_depthRenderingInfo;
		m_geometryRenderingInfo.pStencilAttachment = nullptr;

		// ----------------------------------------------------------------------------
		// lighting
		m_outputAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		m_outputAttach.pNext = VK_NULL_HANDLE;
		m_outputAttach.imageView = m_vkState().m_swapChain.m_swapChainImageViews[m_vkState().m_imageIndex];
		m_outputAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		m_outputAttach.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		m_outputAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		m_lightingRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		m_lightingRenderingInfo.pNext = VK_NULL_HANDLE;
		m_lightingRenderingInfo.flags = 0;
		m_lightingRenderingInfo.renderArea = renderArea;
		m_lightingRenderingInfo.layerCount = 1;
		m_lightingRenderingInfo.viewMask = 0;
		m_lightingRenderingInfo.colorAttachmentCount = 1;
		m_lightingRenderingInfo.pColorAttachments = &m_outputAttach;
		m_lightingRenderingInfo.pDepthAttachment = nullptr;
		m_lightingRenderingInfo.pStencilAttachment = nullptr;

		CreateGeometryPipeline();
		CreateLightingPipeline();

		return false;
	}

	bool RendererDeferred13::OnPrepareNextFrame(Message message) {
		// empty
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
		// Geometry Pass
		auto cmdBuffer = m_commandBuffers[m_vkState().m_currentFrame];
		vvh::ComBeginCommandBuffer({ cmdBuffer });

		vkCmdBeginRendering(cmdBuffer, &m_geometryRenderingInfo);
		RecordObjects(cmdBuffer);
		vkCmdEndRendering(cmdBuffer);

		// ---------------------------------------------------------------------
		// Lighting pass

		PrepareLightingAttachments(cmdBuffer);

		// Changes every frame
		m_outputAttach.imageView = m_vkState().m_swapChain.m_swapChainImageViews[m_vkState().m_imageIndex];

		vkCmdBeginRendering(cmdBuffer, &m_lightingRenderingInfo);
		RecordLighting(cmdBuffer);
		vkCmdEndRendering(cmdBuffer);

		ResetLightingAttachments(cmdBuffer);

		vvh::ComEndCommandBuffer({ .m_commandBuffer = cmdBuffer });
		SubmitCommandBuffer(cmdBuffer);

		return false;
	}

	bool RendererDeferred13::OnWindowSize(Message message) {
		for (size_t i = 0; i < 3; ++i) {
			m_gbufferRenderingInfo[i].imageView = m_gBufferAttachments[i].m_gbufferImageView;
		}
		m_depthRenderingInfo.imageView = m_vkState().m_depthImage.m_depthImageView;

		VkRect2D renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent };
		m_geometryRenderingInfo.renderArea = renderArea;
		m_lightingRenderingInfo.renderArea = renderArea;

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
