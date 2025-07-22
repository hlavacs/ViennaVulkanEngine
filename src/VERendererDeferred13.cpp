#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred13::RendererDeferred13(const std::string& systemName, Engine& engine, const std::string& windowName)
		: RendererDeferredCommon(systemName, engine, windowName) {}

	RendererDeferred13::~RendererDeferred13() {};

	void RendererDeferred13::OnInit() {
		CreateGeometryRenderingInfo();
		CreateGeometryPipeline();

		CreateLightingRenderingInfo();
		CreateLightingPipeline();
	}

	void RendererDeferred13::OnPrepareNextFrame() {}

	void RendererDeferred13::OnObjectCreate() {}

	void RendererDeferred13::OnObjectDestroy() {}

	void RendererDeferred13::OnRecordNextFrame() {
		// Geometry Pass
		auto& cmdBuffer = m_commandBuffers[m_vkState().m_currentFrame];
		vvh::ComBeginCommandBuffer({ cmdBuffer });

		vkCmdBeginRendering(cmdBuffer, &m_geometryRenderingInfo[m_vkState().m_currentFrame]);
		RecordObjects(cmdBuffer);
		vkCmdEndRendering(cmdBuffer);

		// ---------------------------------------------------------------------
		// Lighting pass

		PrepareLightingAttachments(cmdBuffer);

		// Changes every frame
		m_outputAttachmentInfo.imageView = m_vkState().m_swapChain.m_swapChainImageViews[m_vkState().m_imageIndex];

		vkCmdBeginRendering(cmdBuffer, &m_lightingRenderingInfo);
		RecordLighting(cmdBuffer);
		vkCmdEndRendering(cmdBuffer);

		ResetLightingAttachments(cmdBuffer);

		vvh::ComEndCommandBuffer({ .m_commandBuffer = cmdBuffer });
		SubmitCommandBuffer(cmdBuffer);
	}

	void RendererDeferred13::OnWindowSize() {
		VkRect2D renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent };

		for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			for (auto j = 0; j < COUNT - 1; ++j) {
				auto idx = j + i * (COUNT - 1);
				m_gbufferAttachmentInfo[i][j].imageView = m_gBufferAttachments[idx].m_gbufferImageView;
			}
			m_geometryRenderingInfo[i].renderArea = renderArea;
		}
		m_depthAttachmentInfo.imageView = m_vkState().m_depthImage.m_depthImageView;

		m_lightingRenderingInfo.renderArea = renderArea;
	}

	void RendererDeferred13::OnQuit() {}

	void RendererDeferred13::CreateGeometryRenderingInfo() {

		VkRect2D renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent };

		// TODO: depth attachment - view per frame in flight?!
		m_depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		m_depthAttachmentInfo.pNext = VK_NULL_HANDLE;
		m_depthAttachmentInfo.imageView = m_vkState().m_depthImage.m_depthImageView;
		m_depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		m_depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		m_depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		m_depthAttachmentInfo.clearValue = m_clearDepthStencilValue;

		for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			for (auto j = 0; j < COUNT - 1; ++j) {
				auto idx = j + i * (COUNT - 1);
				m_gbufferAttachmentInfo[i][j].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				m_gbufferAttachmentInfo[i][j].pNext = VK_NULL_HANDLE;
				m_gbufferAttachmentInfo[i][j].imageView = m_gBufferAttachments[idx].m_gbufferImageView;
				m_gbufferAttachmentInfo[i][j].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				m_gbufferAttachmentInfo[i][j].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				m_gbufferAttachmentInfo[i][j].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				m_gbufferAttachmentInfo[i][j].clearValue = m_clearColorValue;
			}
			m_geometryRenderingInfo[i].sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			m_geometryRenderingInfo[i].pNext = VK_NULL_HANDLE;
			m_geometryRenderingInfo[i].flags = 0;
			m_geometryRenderingInfo[i].renderArea = renderArea;
			m_geometryRenderingInfo[i].layerCount = 1;
			m_geometryRenderingInfo[i].viewMask = 0;
			m_geometryRenderingInfo[i].colorAttachmentCount = static_cast<uint32_t>(COUNT - 1);
			m_geometryRenderingInfo[i].pColorAttachments = m_gbufferAttachmentInfo[i].data();
			m_geometryRenderingInfo[i].pDepthAttachment = &m_depthAttachmentInfo;
			m_geometryRenderingInfo[i].pStencilAttachment = nullptr;
		}

	}

	void RendererDeferred13::CreateLightingRenderingInfo() {
		m_outputAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		m_outputAttachmentInfo.pNext = VK_NULL_HANDLE;
		m_outputAttachmentInfo.imageView = m_vkState().m_swapChain.m_swapChainImageViews[m_vkState().m_imageIndex];
		m_outputAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		m_outputAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		m_outputAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		VkRect2D renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent };
		m_lightingRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		m_lightingRenderingInfo.pNext = VK_NULL_HANDLE;
		m_lightingRenderingInfo.flags = 0;
		m_lightingRenderingInfo.renderArea = renderArea;
		m_lightingRenderingInfo.layerCount = 1;
		m_lightingRenderingInfo.viewMask = 0;
		m_lightingRenderingInfo.colorAttachmentCount = 1;
		m_lightingRenderingInfo.pColorAttachments = &m_outputAttachmentInfo;
		m_lightingRenderingInfo.pDepthAttachment = nullptr;
		m_lightingRenderingInfo.pStencilAttachment = nullptr;
	}

}	// namespace vve
