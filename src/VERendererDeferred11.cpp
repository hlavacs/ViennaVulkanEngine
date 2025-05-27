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

		CreateGeometryPipeline(m_geometryPass);
		CreateLightingPipeline();
		return false;
	}

	bool RendererDeferred11::OnPrepareNextFrame(Message message) {
		// empty
		return false;
	}

	bool RendererDeferred11::OnRecordNextFrame(Message message) {
		// Geometry Pass
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

		RecordObjects(cmdBuffer, &m_geometryPass);

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

		RecordLighting(cmdBuffer);

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
