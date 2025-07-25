#include "VHInclude.h"
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

		vkCmdBeginRendering(cmdBuffer, &m_lightingRenderingInfo[m_vkState().m_imageIndex]);
		RecordLighting(cmdBuffer);
		vkCmdEndRendering(cmdBuffer);

		ResetLightingAttachments(cmdBuffer);

		vvh::ComEndCommandBuffer({ .m_commandBuffer = cmdBuffer });
		SubmitCommandBuffer(cmdBuffer);
	}

	void RendererDeferred13::OnWindowSize() {
		VkRect2D renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent };

		for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			for (auto j = 0; j < COUNT; ++j) {
				m_gbufferAttachmentInfo[i][j].imageView = m_gBufferAttachments[i][j].m_gbufferImageView;
			}
			m_depthAttachmentInfo[i].imageView = m_depthAttachments[i].m_depthImageView;
			m_geometryRenderingInfo[i].renderArea = renderArea;
		}

		for (size_t i = 0; i < m_vkState().m_swapChain.m_swapChainImageViews.size(); ++i) {
			m_outputAttachmentInfo[i].imageView = m_vkState().m_swapChain.m_swapChainImageViews[i];
			m_lightingRenderingInfo[i].renderArea = renderArea;
		}
	}

	void RendererDeferred13::OnQuit() {}

	void RendererDeferred13::CreateGeometryRenderingInfo() {

		VkRect2D renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent };

		for (auto i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			for (auto j = 0; j < COUNT; ++j) {
				m_gbufferAttachmentInfo[i][j].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
				m_gbufferAttachmentInfo[i][j].pNext = VK_NULL_HANDLE;
				m_gbufferAttachmentInfo[i][j].imageView = m_gBufferAttachments[i][j].m_gbufferImageView;
				m_gbufferAttachmentInfo[i][j].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				m_gbufferAttachmentInfo[i][j].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				m_gbufferAttachmentInfo[i][j].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				m_gbufferAttachmentInfo[i][j].clearValue = m_clearColorValue;
			}

			m_depthAttachmentInfo[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			m_depthAttachmentInfo[i].pNext = VK_NULL_HANDLE;
			m_depthAttachmentInfo[i].imageView = m_depthAttachments[i].m_depthImageView;
			m_depthAttachmentInfo[i].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			m_depthAttachmentInfo[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			m_depthAttachmentInfo[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			m_depthAttachmentInfo[i].clearValue = m_clearDepthStencilValue;

			m_geometryRenderingInfo[i].sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			m_geometryRenderingInfo[i].pNext = VK_NULL_HANDLE;
			m_geometryRenderingInfo[i].flags = 0;
			m_geometryRenderingInfo[i].renderArea = renderArea;
			m_geometryRenderingInfo[i].layerCount = 1;
			m_geometryRenderingInfo[i].viewMask = 0;
			m_geometryRenderingInfo[i].colorAttachmentCount = static_cast<uint32_t>(COUNT);
			m_geometryRenderingInfo[i].pColorAttachments = m_gbufferAttachmentInfo[i].data();
			m_geometryRenderingInfo[i].pDepthAttachment = &m_depthAttachmentInfo[i];
			m_geometryRenderingInfo[i].pStencilAttachment = nullptr;
		}

	}

	void RendererDeferred13::CreateLightingRenderingInfo() {
		m_outputAttachmentInfo.resize(m_vkState().m_swapChain.m_swapChainImageViews.size());
		m_lightingRenderingInfo.resize(m_vkState().m_swapChain.m_swapChainImageViews.size());
		VkRect2D renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent };

		for (size_t i = 0; i < m_vkState().m_swapChain.m_swapChainImageViews.size(); ++i) {
			m_outputAttachmentInfo[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			m_outputAttachmentInfo[i].pNext = VK_NULL_HANDLE;
			m_outputAttachmentInfo[i].imageView = m_vkState().m_swapChain.m_swapChainImageViews[i];
			m_outputAttachmentInfo[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			m_outputAttachmentInfo[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			m_outputAttachmentInfo[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

			m_lightingRenderingInfo[i].sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
			m_lightingRenderingInfo[i].pNext = VK_NULL_HANDLE;
			m_lightingRenderingInfo[i].flags = 0;
			m_lightingRenderingInfo[i].renderArea = renderArea;
			m_lightingRenderingInfo[i].layerCount = 1;
			m_lightingRenderingInfo[i].viewMask = 0;
			m_lightingRenderingInfo[i].colorAttachmentCount = 1;
			m_lightingRenderingInfo[i].pColorAttachments = &m_outputAttachmentInfo[i];
			m_lightingRenderingInfo[i].pDepthAttachment = nullptr;
			m_lightingRenderingInfo[i].pStencilAttachment = nullptr;
		}
	}

}	// namespace vve
