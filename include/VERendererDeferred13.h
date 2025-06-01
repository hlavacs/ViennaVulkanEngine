#pragma once

namespace vve {

	class RendererDeferred13 : public RendererDeferredCommon<RendererDeferred13> {

		friend class RendererDeferredCommon<RendererDeferred13>;

	public:
		RendererDeferred13(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferred13();

	private:
		void OnInit(Message message);
		void OnPrepareNextFrame(Message message);
		void OnRecordNextFrame(Message message);
		void OnObjectCreate(Message message);
		void OnObjectDestroy(Message message);
		void OnWindowSize(Message message);
		void OnQuit(Message message);

		void CreateGeometryRenderingInfo();
		void CreateLightingRenderingInfo();

		VkRenderingAttachmentInfo m_gbufferRenderingInfo[COUNT - 1]{};
		VkRenderingAttachmentInfo m_depthRenderingInfo{};
		VkRenderingInfo m_geometryRenderingInfo{};

		VkRenderingAttachmentInfo m_outputAttach{};
		VkRenderingInfo m_lightingRenderingInfo{};

	};

}	// namespace vve
