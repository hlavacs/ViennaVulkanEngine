/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/

#ifndef VERENDERER_RAYTRACING_NV_H
#define VERENDERER_RAYTRACING_NV_H

namespace ve
{
	class VEEngine;

	class VESubrenderRayTracingNV_DN;

	/**
		*
		* \brief Ray Tracing Renderer based on NV Ray Tracing Extension
		*
		*/
	class VERendererRayTracingNV : public VERenderer
	{
		friend VESubrenderRayTracingNV_DN;

	public:
		///\brief One secondary command buffer and the pool that it came from
		struct secondaryCmdBuf_t
		{
			VkCommandBuffer buffer; ///<Vulkan cmd buffer handle
			VkCommandPool pool; ///<Vulkan cmd buffer pool handle
			secondaryCmdBuf_t &operator=(const secondaryCmdBuf_t &right)
			{ ///<copy operator
				buffer = right.buffer;
				pool = right.pool;
				return *this;
			};
		};

		VERendererRayTracingNV();

		virtual void addEntityToSubrenderer(VEEntity *pEntity) override;

		VkPhysicalDeviceRayTracingPropertiesNV getPhysicalDeviceRTProperties()
		{
			return m_raytracingProperties;
		}

		///\returns the depth map vector
		VETexture *getDepthMap()
		{
			return m_depthMap;
		};

	protected:
		virtual void createSubrenderers(); //create the subrenderers

		virtual void updateTLAS();

		VkAccelerationStructureNV *getTLASHandlePointer()
		{
			return &m_topLevelAS.handleNV;
		};

		void createSyncObjects(); //create the sync objects
		void cleanupSwapChain(); //delete the swapchain

		virtual void initRenderer(); //init the renderer
		virtual void recordCmdBuffers(); //record the command buffers

		virtual void drawFrame(); //draw one frame
		virtual void prepareOverlay(); //prepare to draw the overlay
		virtual void drawOverlay(); //Draw the overlay (GUI)
		virtual void presentFrame(); //Present the newly drawn frame
		virtual void closeRenderer(); //close the renderer
		virtual void recreateSwapchain(); //new swapchain due to window size change
		virtual secondaryCmdBuf_t recordRenderpass(std::vector<VESubrender *> subRenderers,
			VkFramebuffer *pFrameBuffer,
			uint32_t imageIndex,
			uint32_t numPass,
			VECamera *pCamera,
			VELight *pLight);

		///\returns the command pool for this thread - each threads needs its own pool
		virtual VkCommandPool
			getThreadCommandPool()
		{
			return m_commandPools[getEnginePointer()->getThreadPool()->threadNum[std::this_thread::get_id()]];
		};

		virtual void updateCmdBuffers();

		virtual void deleteCmdBuffers();

		VETexture *m_depthMap = nullptr; ///<the image depth map
		std::vector<VkSemaphore> m_imageAvailableSemaphores; ///<sem for waiting for the next swapchain image
		std::vector<VkSemaphore> m_renderFinishedSemaphores; ///<sem for signalling that rendering done
		std::vector<VkSemaphore> m_overlaySemaphores; ///<sem for signalling that rendering done
		std::vector<VkFence> m_inFlightFences; ///<fences for halting the next image render until this one is done
		size_t m_currentFrame = 0; ///<int for the fences
		bool m_framebufferResized = false; ///<signal that window size is changing

		std::vector<VkCommandPool> m_commandPools = {}; ///<Array of command pools so that each thread in the thread pool has its own pool
		std::vector<VkCommandBuffer> m_commandBuffers = {}; ///<the main command buffers for recording draw commands

		VkRenderPass m_renderPass; ///<The first light render pass, clearing the framebuffers
		
		std::vector<std::vector<std::future<secondaryCmdBuf_t>>>
			m_secondaryBuffersFutures = {}; ///<secondary buffers for parallel recording
		std::vector<std::vector<secondaryCmdBuf_t>> m_secondaryBuffers = {}; ///<secondary buffers for parallel recording

		vh::vhAccelerationStructure m_topLevelAS;

		VkPhysicalDeviceRayTracingPropertiesNV m_raytracingProperties = {};
	};
} // namespace ve
#endif // VERENDERER_NV_RT_H