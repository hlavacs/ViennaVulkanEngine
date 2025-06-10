#pragma once

namespace vve {

	class RendererDeferred13 : public RendererDeferredCommon<RendererDeferred13> {

		friend class RendererDeferredCommon<RendererDeferred13>;

	public:
		RendererDeferred13(const std::string& systemName, Engine& engine, const std::string& windowName);
		virtual ~RendererDeferred13();

	private:
		void OnInit();
		void OnPrepareNextFrame();
		void OnRecordNextFrame();
		void OnObjectCreate();
		void OnObjectDestroy();
		void OnWindowSize();
		void OnQuit();

		void CreateGeometryRenderingInfo();
		void CreateLightingRenderingInfo();

		VkRenderingAttachmentInfo m_gbufferRenderingInfo[COUNT - 1]{};
		VkRenderingAttachmentInfo m_depthRenderingInfo{};
		VkRenderingInfo m_geometryRenderingInfo{};

		VkRenderingAttachmentInfo m_outputAttach{};
		VkRenderingInfo m_lightingRenderingInfo{};

	};

}	// namespace vve
