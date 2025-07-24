#pragma once

namespace vve {

	class RendererDeferred11 : public RendererDeferredCommon<RendererDeferred11> {

		friend class RendererDeferredCommon<RendererDeferred11>;

	public:
		RendererDeferred11(const std::string& systemName, Engine& engine, const std::string& windowName);
		virtual ~RendererDeferred11();

	private:
		void OnInit();
		void OnPrepareNextFrame();
		void OnRecordNextFrame();
		void OnObjectCreate();
		void OnObjectDestroy();
		void OnWindowSize();
		void OnQuit();

		void CreateDeferredFrameBuffers();
		void DestroyDeferredFrameBuffers();

		std::vector<VkFramebuffer> m_gBufferFrameBuffers{ VK_NULL_HANDLE };
		std::vector<VkFramebuffer> m_lightingFrameBuffers{ VK_NULL_HANDLE };

		VkRenderPass m_geometryPass{ VK_NULL_HANDLE };
		VkRenderPass m_lightingPass{ VK_NULL_HANDLE };

		std::array<VkClearValue, COUNT + 1> m_clearValues{};
	};

}	// namespace vve
