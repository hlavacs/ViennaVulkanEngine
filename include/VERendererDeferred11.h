#pragma once

namespace vve {

	class RendererDeferred11 : public RendererDeferredCommon<RendererDeferred11> {

		friend class RendererDeferredCommon<RendererDeferred11>;

	public:
		RendererDeferred11(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferred11();

	private:
		void OnInit(Message message);
		void OnPrepareNextFrame(Message message);
		void OnRecordNextFrame(Message message);
		void OnObjectCreate(Message message);
		void OnObjectDestroy(Message message);
		void OnWindowSize(Message message);
		void OnQuit(Message message);

		void CreateDeferredFrameBuffers();
		void DestroyDeferredFrameBuffers();

		std::vector<VkFramebuffer> m_gBufferFrameBuffers{};
		std::vector<VkFramebuffer> m_lightingFrameBuffers{};

		VkRenderPass m_geometryPass{ VK_NULL_HANDLE };
		VkRenderPass m_lightingPass{ VK_NULL_HANDLE };

	};

}	// namespace vve
