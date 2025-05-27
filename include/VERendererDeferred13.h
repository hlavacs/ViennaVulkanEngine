#pragma once

namespace vve {

	class RendererDeferred13 : public RendererDeferredCommon<RendererDeferred13> {

		friend class RendererDeferredCommon<RendererDeferred13>;

	public:
		RendererDeferred13(std::string systemName, Engine& engine, std::string windowName);
		virtual ~RendererDeferred13();

	private:
		bool OnInit(Message message);
		bool OnPrepareNextFrame(Message message);
		bool OnRecordNextFrame(Message message);
		bool OnObjectCreate(Message message);
		bool OnObjectDestroy(Message message);
		bool OnWindowSize(Message message);
		bool OnQuit(Message message);

		void CreateLightingPipeline();

		VkRenderingAttachmentInfo m_gbufferRenderingInfo[3]{};
		VkRenderingAttachmentInfo m_depthRenderingInfo{};
		VkRenderingInfo m_geometryRenderingInfo{};

		VkRenderingAttachmentInfo m_outputAttach{};
		VkRenderingInfo m_lightingRenderingInfo{};

	};

}	// namespace vve
