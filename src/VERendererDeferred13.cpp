#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

	/**
	 * @brief Constructs a Vulkan 1.3 deferred renderer
	 * @param systemName Name of the renderer system
	 * @param engine Reference to the engine instance
	 * @param windowName Name of the window to render to
	 */
	RendererDeferred13::RendererDeferred13(const std::string& systemName, Engine& engine, const std::string& windowName)
		: RendererDeferredCommon(systemName, engine, windowName) {}

	/**
	 * @brief Destructor for the Vulkan 1.3 deferred renderer
	 */
	RendererDeferred13::~RendererDeferred13() {};

	/**
	 * @brief Initializes the Vulkan 1.3 deferred renderer with dynamic rendering
	 */
	void RendererDeferred13::OnInit() {
		CreateGeometryRenderingInfo();
		CreateGeometryPipeline();

		CreateLightingRenderingInfo();
		CreateLightingPipeline();
	}

	/**
	 * @brief Prepares resources for the next frame (empty implementation for this renderer)
	 */
	void RendererDeferred13::OnPrepareNextFrame() {}

	/**
	 * @brief Handles object creation events (empty implementation for this renderer)
	 */
	void RendererDeferred13::OnObjectCreate() {}

	/**
	 * @brief Handles object destruction events (empty implementation for this renderer)
	 */
	void RendererDeferred13::OnObjectDestroy() {}

	/**
	 * @brief Records rendering commands for the next frame using dynamic rendering
	 */
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

	/**
	 * @brief Handles window resize by updating rendering info structures
	 */
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

	/**
	 * @brief Handles application quit (empty implementation for this renderer)
	 */
	void RendererDeferred13::OnQuit() {}

	/**
	 * @brief Creates the rendering info structures for the geometry pass
	 */
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

	/**
	 * @brief Creates the rendering info structures for the lighting composition pass
	 */
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
