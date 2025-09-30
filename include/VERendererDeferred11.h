#pragma once

namespace vve {

	/**
	 * @brief Deferred renderer implementation for Vulkan 1.1
	 */
	class RendererDeferred11 : public RendererDeferredCommon<RendererDeferred11> {

		friend class RendererDeferredCommon<RendererDeferred11>;

	public:
		/**
		 * @brief Constructor for Deferred 1.1 Renderer
		 * @param systemName Name of the system
		 * @param engine Reference to the engine
		 * @param windowName Name of the associated window
		 */
		RendererDeferred11(const std::string& systemName, Engine& engine, const std::string& windowName);
		/**
		 * @brief Destructor for Deferred 1.1 Renderer
		 */
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

		std::array<VkFramebuffer, MAX_FRAMES_IN_FLIGHT> m_gBufferFrameBuffers{};
		std::vector<VkFramebuffer> m_lightingFrameBuffers{ VK_NULL_HANDLE };

		VkRenderPass m_geometryPass{ VK_NULL_HANDLE };
		VkRenderPass m_lightingPass{ VK_NULL_HANDLE };

		std::array<VkClearValue, COUNT + 1> m_clearValues{};
	};

}	// namespace vve
