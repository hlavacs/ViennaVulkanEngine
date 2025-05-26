#pragma once

namespace vve {

	class RendererDeferred11 : public RendererDeferredCommon<RendererDeferred11> {

		friend class RendererDeferredCommon<RendererDeferred11>;

	public:
		RendererDeferred11(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferred11();

	private:
		bool OnInit(Message message);
		bool OnPrepareNextFrame(Message message);
		bool OnRecordNextFrame(Message message);
		bool OnObjectCreate(Message message);
		bool OnObjectDestroy(Message message);
		bool OnWindowSize(Message message);
		bool OnQuit(Message message);

		void CreateGeometryPipeline();
		void CreateLightingPipeline();

		void CreateDeferredFrameBuffers();
		void DestroyDeferredFrameBuffers();

		std::vector<VkFramebuffer> m_gBufferFrameBuffers{};
		std::vector<VkFramebuffer> m_lightingFrameBuffers{};

		VkRenderPass m_geometryPass{ VK_NULL_HANDLE };
		VkRenderPass m_lightingPass{ VK_NULL_HANDLE };

	};

}	// namespace vve
