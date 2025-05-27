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
		CreateLightingPipeline(m_lightingPass);
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
