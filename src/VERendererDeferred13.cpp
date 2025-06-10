#include "VHInclude2.h"
#include "VEInclude.h"

namespace vve {

	RendererDeferred13::RendererDeferred13(const std::string& systemName, Engine& engine, const std::string& windowName)
		: RendererDeferredCommon(systemName, engine, windowName) {}

	RendererDeferred13::~RendererDeferred13() {};

	void RendererDeferred13::OnInit(Message message) {
		CreateGeometryRenderingInfo();
		CreateGeometryPipeline();

		CreateLightingRenderingInfo();
		CreateLightingPipeline();
	}

	void RendererDeferred13::OnPrepareNextFrame(Message message) {}

	void RendererDeferred13::OnObjectCreate(Message message) {}

	void RendererDeferred13::OnObjectDestroy(Message message) {}

	void RendererDeferred13::OnRecordNextFrame(Message message) {
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
	}

	void RendererDeferred13::OnWindowSize(Message message) {
		for (size_t i = 0; i < m_gBufferAttachments.size(); ++i) {
			m_gbufferRenderingInfo[i].imageView = m_gBufferAttachments[i].m_gbufferImageView;
		}
		m_depthRenderingInfo.imageView = m_vkState().m_depthImage.m_depthImageView;

		VkRect2D renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent };
		m_geometryRenderingInfo.renderArea = renderArea;
		m_lightingRenderingInfo.renderArea = renderArea;
	}

	void RendererDeferred13::OnQuit(Message message) {}

	void RendererDeferred13::CreateGeometryRenderingInfo() {
		// TODO: Maybe add KHR way for extension support?
		size_t i = 0;
		for (const auto& attach : m_gBufferAttachments) {
			m_gbufferRenderingInfo[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
			m_gbufferRenderingInfo[i].pNext = VK_NULL_HANDLE;
			m_gbufferRenderingInfo[i].imageView = attach.m_gbufferImageView;
			m_gbufferRenderingInfo[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			m_gbufferRenderingInfo[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			m_gbufferRenderingInfo[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			m_gbufferRenderingInfo[i].clearValue = m_clearColorValue;
			i++;
		}

		m_depthRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		m_depthRenderingInfo.pNext = VK_NULL_HANDLE;
		m_depthRenderingInfo.imageView = m_vkState().m_depthImage.m_depthImageView;
		m_depthRenderingInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		m_depthRenderingInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		m_depthRenderingInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		m_depthRenderingInfo.clearValue = m_clearDepthStencilValue;

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
	}

	void RendererDeferred13::CreateLightingRenderingInfo() {
		m_outputAttach.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		m_outputAttach.pNext = VK_NULL_HANDLE;
		m_outputAttach.imageView = m_vkState().m_swapChain.m_swapChainImageViews[m_vkState().m_imageIndex];
		m_outputAttach.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		m_outputAttach.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		m_outputAttach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		VkRect2D renderArea = VkRect2D{ VkOffset2D{}, m_vkState().m_swapChain.m_swapChainExtent };
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
	}

}	// namespace vve
