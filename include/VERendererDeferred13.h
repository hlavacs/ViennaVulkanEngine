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

		std::array<std::array<VkRenderingAttachmentInfo, COUNT - 1>, MAX_FRAMES_IN_FLIGHT> m_gbufferAttachmentInfo{};
		VkRenderingAttachmentInfo m_depthAttachmentInfo{};
		VkRenderingInfo m_geometryRenderingInfo[MAX_FRAMES_IN_FLIGHT]{};

		VkRenderingAttachmentInfo m_outputAttachmentInfo{};
		VkRenderingInfo m_lightingRenderingInfo{};

	};

}	// namespace vve
