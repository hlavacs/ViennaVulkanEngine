#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred11::RendererDeferred11(std::string systemName, Engine& engine, std::string windowName)
		: RendererDeferredCommon(systemName, engine, windowName) {}

	RendererDeferred11::~RendererDeferred11() {};

	bool RendererDeferred11::OnInit(Message message) {

		vvh::RenCreateRenderPassGeometry({
			.m_depthFormat			= m_vkState().m_depthMapFormat,
			.m_device				= m_vkState().m_device,
			.m_swapChain			= m_vkState().m_swapChain,
			.m_clear				= true,
			.m_renderPass			= m_geometryPass
			});
		vvh::RenCreateRenderPassLighting({
			.m_device				= m_vkState().m_device,
			.m_swapChain			= m_vkState().m_swapChain,
			.m_clear				= false,
			.m_renderPass			= m_lightingPass
			});

		CreateDeferredFrameBuffers();

		CreateGeometryPipeline();
		CreateLightingPipeline();
		return false;
	}

	bool RendererDeferred11::OnPrepareNextFrame(Message message) {
		// empty
		return false;
	}

	bool RendererDeferred11::OnRecordNextFrame(Message message) {

		auto cmdBuffer = m_commandBuffers[m_vkState().m_currentFrame];
		vvh::ComBeginCommandBuffer({ cmdBuffer });

		vvh::ComBeginRenderPass2({ 
			.m_commandBuffer		= cmdBuffer,
			.m_imageIndex			= m_vkState().m_imageIndex,
			.m_swapChain			= m_vkState().m_swapChain,
			.m_gBufferFramebuffers	= m_gBufferFrameBuffers,
			.m_renderPass			= m_geometryPass,
			.m_clearValues			= m_clearValues,
			.m_currentFrame			= m_vkState().m_currentFrame 
			});

		for (auto& pipeline : m_geomPipesPerType) {

			vvh::Pipeline pip{
				pipeline.second.m_graphicsPipeline.m_pipelineLayout,
				pipeline.second.m_graphicsPipeline.m_pipeline
			};

			vvh::ComBindPipeline({
				.m_commandBuffer	= cmdBuffer,
				.m_graphicsPipeline	= pip,
				.m_imageIndex		= m_vkState().m_imageIndex,
				.m_swapChain		= m_vkState().m_swapChain,
				.m_renderPass		= m_geometryPass,
				.m_viewPorts		= {},
				.m_scissors			= {}, //default view ports and scissors
				.m_blendConstants	= {},
				.m_pushConstants	= {},
				.m_currentFrame		= m_vkState().m_currentFrame
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
					.m_commandBuffer	= cmdBuffer,
					.m_graphicsPipeline = pipeline.second.m_graphicsPipeline,
					.m_descriptorSets	= { m_descriptorSetPerFrame, descriptorsets },
					.m_type				= pipeline.second.m_type,
					.m_mesh				= mesh,
					.m_currentFrame		= m_vkState().m_currentFrame
					});
			}
		}
		vvh::ComEndRenderPass({ .m_commandBuffer = cmdBuffer });

		// ---------------------------------------------------------------------
		// Lighting pass

		PrepareLightingAttachments(cmdBuffer);

		vvh::ComBeginRenderPass2({
			.m_commandBuffer		= cmdBuffer,
			.m_imageIndex			= m_vkState().m_imageIndex,
			.m_swapChain			= m_vkState().m_swapChain,
			.m_gBufferFramebuffers	= m_lightingFrameBuffers,
			.m_renderPass			= m_lightingPass,
			.m_clearValues			= {},
			.m_currentFrame			= m_vkState().m_currentFrame
			});

		vvh::LightOffset offset{ 0, m_numberLightsPerType.x + m_numberLightsPerType.y + m_numberLightsPerType.z };

		vvh::ComBindPipeline({
			.m_commandBuffer	= cmdBuffer,
			.m_graphicsPipeline = m_lightingPipeline,
			.m_imageIndex		= m_vkState().m_imageIndex,
			.m_swapChain		= m_vkState().m_swapChain,
			.m_renderPass		= m_lightingPass,
			.m_viewPorts		= {},
			.m_scissors			= {}, //default view ports and scissors
			.m_blendConstants	= {},
			.m_pushConstants	= {
				{
					.layout		= m_lightingPipeline.m_pipelineLayout,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.offset		= 0,
					.size		= sizeof(offset),
					.pValues	= &offset
				}
			},
			.m_currentFrame		= m_vkState().m_currentFrame
			});

		VkDescriptorSet sets[] = { m_descriptorSetPerFrame.m_descriptorSetPerFrameInFlight[m_vkState().m_currentFrame],
			m_descriptorSetComposition.m_descriptorSetPerFrameInFlight[m_vkState().m_currentFrame] };

		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline.m_pipelineLayout,
			0, 2, sets, 0, nullptr);

		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

		vvh::ComEndRenderPass({ .m_commandBuffer = cmdBuffer });

		ResetLightingAttachments(cmdBuffer);

		vvh::ComEndCommandBuffer({ .m_commandBuffer = cmdBuffer });
		SubmitCommandBuffer(cmdBuffer);

		return false;
	}

	bool RendererDeferred11::OnObjectCreate(Message message) {
		// empty
		return false;
	}

	bool RendererDeferred11::OnObjectDestroy(Message message) {
		// empty
		return false;
	}

	bool RendererDeferred11::OnWindowSize(Message message) {
		DestroyDeferredFrameBuffers();
		CreateDeferredFrameBuffers();

		return false;
	}

	bool RendererDeferred11::OnQuit(Message message) {

		vkDestroyRenderPass(m_vkState().m_device, m_geometryPass, nullptr);
		vkDestroyRenderPass(m_vkState().m_device, m_lightingPass, nullptr);
		DestroyDeferredFrameBuffers();

		return false;
	}

	void RendererDeferred11::CreateGeometryPipeline() {
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
					.m_device				= m_vkState().m_device,
					.m_bindings				= bindings,
					.m_descriptorSetLayout	= descriptorSetLayoutPerObject
					});

				std::vector<VkVertexInputBindingDescription> bindingDescriptions = getBindingDescriptions(type);
				std::vector<VkVertexInputAttributeDescription> attributeDescriptions = getAttributeDescriptions(type);

				VkPipelineColorBlendAttachmentState colorBlendAttachment{};
				// TODO: colorBlendAttachment.colorWriteMask = 0xf; ???
				// TODO: rewrite to make use for the 3 attachments clearer
				colorBlendAttachment.colorWriteMask			= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
				colorBlendAttachment.srcColorBlendFactor	= VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
				colorBlendAttachment.dstColorBlendFactor	= VK_BLEND_FACTOR_CONSTANT_COLOR;
				colorBlendAttachment.colorBlendOp			= VK_BLEND_OP_ADD;
				colorBlendAttachment.srcAlphaBlendFactor	= VK_BLEND_FACTOR_CONSTANT_ALPHA;
				colorBlendAttachment.dstAlphaBlendFactor	= VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
				colorBlendAttachment.alphaBlendOp			= VK_BLEND_OP_MAX;
				colorBlendAttachment.blendEnable			= VK_TRUE;

				vvh::RenCreateGraphicsPipeline({ 
					.m_device					= m_vkState().m_device,
					.m_renderPass				= m_geometryPass,
					.m_vertShaderPath			= entry.path().string(),
					.m_fragShaderPath			= entry.path().string(),
					.m_bindingDescription		= bindingDescriptions,
					.m_attributeDescriptions	= attributeDescriptions,
					.m_descriptorSetLayouts		= { m_descriptorSetLayoutPerFrame, descriptorSetLayoutPerObject },
					.m_specializationConstants	= {},
					.m_pushConstantRanges		= {},
					.m_blendAttachments			= { colorBlendAttachment, colorBlendAttachment, colorBlendAttachment },
					.m_graphicsPipeline			= graphicsPipeline
					// TODO: Check if this change affects something, was: bool depthWrite
					/*true*/
					});

				m_geomPipesPerType[pri] = { type, descriptorSetLayoutPerObject, graphicsPipeline };
			}
		}
	}

	void RendererDeferred11::CreateLightingPipeline() {
		const std::filesystem::path shaders{ "shaders/Deferred" };
		if (!std::filesystem::exists(shaders)) {
			std::cerr << "ERROR: Folder does not exist: " << std::filesystem::absolute(shaders) << "\n";
		}
		const std::string vert = (shaders / "test_lighting.spv").string();
		const std::string frag = (shaders / "test_lighting.spv").string();

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		// TODO: colorBlendAttachment.colorWriteMask = 0xf; ???
		// TODO: rewrite to make use for the 3 attachments clearer
		colorBlendAttachment.colorWriteMask			= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.srcColorBlendFactor	= VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
		colorBlendAttachment.dstColorBlendFactor	= VK_BLEND_FACTOR_CONSTANT_COLOR;
		colorBlendAttachment.colorBlendOp			= VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor	= VK_BLEND_FACTOR_CONSTANT_ALPHA;
		colorBlendAttachment.dstAlphaBlendFactor	= VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
		colorBlendAttachment.alphaBlendOp			= VK_BLEND_OP_MAX;
		colorBlendAttachment.blendEnable			= VK_TRUE;

		vvh::RenCreateGraphicsPipeline({ 
			.m_device					= m_vkState().m_device,
			.m_renderPass				= m_lightingPass,
			.m_vertShaderPath			= vert,
			.m_fragShaderPath			= frag,
			.m_bindingDescription		= {},
			.m_attributeDescriptions	= {},
			.m_descriptorSetLayouts		= { m_descriptorSetLayoutPerFrame, m_descriptorSetLayoutComposition },
			.m_specializationConstants	= { MAX_NUMBER_LIGHTS },
			.m_pushConstantRanges		= { {.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = 8} },
			.m_blendAttachments			= { colorBlendAttachment },
			.m_graphicsPipeline			= m_lightingPipeline,
			// TODO : Check if this change affects something, was : bool depthWrite
			.m_depthWrite				= false
			});
	}

	void RendererDeferred11::CreateDeferredFrameBuffers() {
		// GBuffer FrameBuffers
		vvh::RenCreateGBufferFrameBuffers({
			.m_device				= m_vkState().m_device,
			.m_swapChain			= m_vkState().m_swapChain,
			.m_gBufferAttachs		= m_gBufferAttachments,
			.m_gBufferFrameBuffers	= m_gBufferFrameBuffers,
			.m_depthImage			= m_vkState().m_depthImage,
			.m_renderPass			= m_geometryPass
			});

		// Lighting pass FrameBuffers
		vvh::RenCreateFrameBuffers2({
			.m_device				= m_vkState().m_device,
			.m_renderPass			= m_lightingPass,
			.m_swapChain			= m_vkState().m_swapChain,
			.m_frameBuffers			= m_lightingFrameBuffers
			});
	}

	void RendererDeferred11::DestroyDeferredFrameBuffers() {
		for (auto& fb : m_gBufferFrameBuffers) {
			vkDestroyFramebuffer(m_vkState().m_device, fb, nullptr);
		}
		for (auto& fb : m_lightingFrameBuffers) {
			vkDestroyFramebuffer(m_vkState().m_device, fb, nullptr);
		}
	}

}	// namespace vve
