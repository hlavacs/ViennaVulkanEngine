#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred11::RendererDeferred11(const std::string& systemName, Engine& engine, const std::string& windowName)
		: RendererDeferredCommon(systemName, engine, windowName) {}

	RendererDeferred11::~RendererDeferred11() {};

	void RendererDeferred11::OnInit() {
		vvh::RenCreateRenderPassGeometry({
			.m_depthFormat			= m_vkState().m_depthMapFormat,
			.m_device				= m_vkState().m_device,
			.m_swapChain			= m_vkState().m_swapChain,
			.m_clear				= true,
			.m_renderPass			= m_geometryPass,
			.m_formats				= getAttachmentFormats()
			});
		vvh::RenCreateRenderPassLighting({
			.m_device				= m_vkState().m_device,
			.m_swapChain			= m_vkState().m_swapChain,
			.m_clear				= false,
			.m_renderPass			= m_lightingPass
			});

		for (uint8_t i = 0; i < COUNT; ++i) m_clearValues[i] = m_clearColorValue;
		m_clearValues[COUNT] = m_clearDepthStencilValue;

		CreateDeferredFrameBuffers();

		CreateGeometryPipeline(&m_geometryPass);
		CreateLightingPipeline(&m_lightingPass);
	}

	void RendererDeferred11::OnPrepareNextFrame() {}

	void RendererDeferred11::OnRecordNextFrame() {
		// Geometry Pass
		auto& cmdBuffer = m_commandBuffers[m_vkState().m_currentFrame];
		vvh::ComBeginCommandBuffer({ cmdBuffer });

		vvh::ComBeginRenderPass2({ 
			.m_commandBuffer		= cmdBuffer,
			.m_imageIndex			= m_vkState().m_currentFrame,
			.m_extent				= m_vkState().m_swapChain.m_swapChainExtent,
			.m_framebuffers			= m_gBufferFrameBuffers,
			.m_renderPass			= m_geometryPass,
			.m_clearValues			= m_clearValues
			});

		RecordObjects(cmdBuffer, &m_geometryPass);

		vvh::ComEndRenderPass({ .m_commandBuffer = cmdBuffer });

		// ---------------------------------------------------------------------
		// Lighting pass

		PrepareLightingAttachments(cmdBuffer);

		vvh::ComBeginRenderPass2({
			.m_commandBuffer		= cmdBuffer,
			.m_imageIndex			= m_vkState().m_imageIndex,
			.m_extent				= m_vkState().m_swapChain.m_swapChainExtent,
			.m_framebuffers			= m_lightingFrameBuffers,
			.m_renderPass			= m_lightingPass,
			.m_clearValues			= {}
			});

		RecordLighting(cmdBuffer);

		vvh::ComEndRenderPass({ .m_commandBuffer = cmdBuffer });

		ResetLightingAttachments(cmdBuffer);

		vvh::ComEndCommandBuffer({ .m_commandBuffer = cmdBuffer });
		SubmitCommandBuffer(cmdBuffer);
	}

	void RendererDeferred11::OnObjectCreate() {}

	void RendererDeferred11::OnObjectDestroy() {}

	void RendererDeferred11::OnWindowSize() {
		DestroyDeferredFrameBuffers();
		CreateDeferredFrameBuffers();
	}

	void RendererDeferred11::OnQuit() {
		vkDestroyRenderPass(m_vkState().m_device, m_geometryPass, nullptr);
		vkDestroyRenderPass(m_vkState().m_device, m_lightingPass, nullptr);
		DestroyDeferredFrameBuffers();
	}

	void RendererDeferred11::CreateDeferredFrameBuffers() {
		// GBuffer FrameBuffers
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vvh::RenCreateGBufferFrameBuffers({
				.m_device = m_vkState().m_device,
				.m_extent = m_vkState().m_swapChain.m_swapChainExtent,
				.m_gBufferAttachs = m_gBufferAttachments[i],
				.m_gBufferFrameBuffer = m_gBufferFrameBuffers[i],
				.m_depthImage = m_vkState().m_depthImage,
				.m_renderPass = m_geometryPass
			});
		}
		
		// Lighting pass FrameBuffers
		m_lightingFrameBuffers.resize(m_vkState().m_swapChain.m_swapChainImageViews.size());
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
