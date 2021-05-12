/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/

#ifndef VERENDERER_RAYTRACING_KHR_H
#define VERENDERER_RAYTRACING_KHR_H

#include <utility>      // std::pair, std::make_pair

namespace ve {

	class VEEngine;
	class VESubrenderRayTracingKHR_DN;

	/**
	*
	* \brief Ray Tracing Renderer based on NV Ray Tracing Extension
	*
	*/
	class VERendererRayTracingKHR : public VERenderer {
		friend VESubrenderRayTracingKHR_DN;
	public:

		// Ray tracing acceleration structure
		struct AccelerationStructure {
			VkAccelerationStructureKHR handle;
			uint64_t deviceAddress = 0;
			VmaAllocation allocation;
			VkBuffer buffer;
		};


		///\brief One secondary command buffer and the pool that it came from
		struct secondaryCmdBuf_t {
			VkCommandBuffer buffer;										///<Vulkan cmd buffer handle
			VkCommandPool pool;											///<Vulkan cmd buffer pool handle
			secondaryCmdBuf_t &operator= (const secondaryCmdBuf_t &right) {	///<copy operator
				buffer = right.buffer;
				pool = right.pool;
				return *this;
			};
		};

		float m_AvgCmdShadowTime = 0.0f;			///<Average time for recording shadow maps
		float m_AvgCmdLightTime = 0.0f;				///<Average time for recording light pass
		float m_AvgRecordTime = 0.0f;				///<Average recording time of one command buffer

		VERendererRayTracingKHR();

		virtual void addEntityToSubrenderer(VEEntity *pEntity) override;
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR getPhysicalDeviceRTProperties() { return m_raytracingProperties; }
		///\returns pointer to the swap chain framebuffer vector
		virtual std::vector<VkFramebuffer> &getSwapChainFrameBuffers() { return m_swapChainFramebuffers; };
		///\returns the depth map vector
		VETexture *getDepthMap() { return m_depthMap; };
		VkAccelerationStructureKHR* getTLASHandlePointer() { return &m_topLevelAS.handle; };

	protected:

		struct RayTracingScratchBuffer
		{
			uint64_t deviceAddress = 0;
			VkBuffer buffer = VK_NULL_HANDLE;
			void *mapped = nullptr;
			VmaAllocation allocation;
		};

		virtual void createSubrenderers();			//create the subrenderers
		virtual void addSubrenderer(VESubrender *pSub);

		virtual void initAccelerationStructures();
		virtual void createAccelerationStructures();
		virtual void createBottomLevelAS(VkCommandBuffer commandBuffer, VEEntity *entity);
		virtual void createTopLevelAS(VkCommandBuffer commandBuffer);

		virtual void createAccelerationStructure(AccelerationStructure &accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo);
		virtual void destroyAccelerationStructure(const AccelerationStructure &as);

		virtual RayTracingScratchBuffer createScratchBuffer(VkDeviceSize size);
		virtual void deleteScratchBuffer(RayTracingScratchBuffer &scratchBuffer);
		
		virtual uint64_t getBufferDeviceAddress(VkBuffer buffer);

		virtual void createSyncObjects();			//create the sync objects
		virtual void cleanupSwapChain();			//delete the swapchain

		virtual void initRenderer();				//init the renderer
		virtual void recordCmdBuffers();			//record the command buffers

		virtual void drawFrame();					//draw one frame
		virtual void prepareOverlay();				//prepare to draw the overlay
		virtual void drawOverlay();					//Draw the overlay (GUI)
		virtual void presentFrame();				//Present the newly drawn frame
		virtual void closeRenderer();				//close the renderer
		virtual void recreateSwapchain();			//new swapchain due to window size change
		virtual secondaryCmdBuf_t recordRenderpass(VkRenderPass *pRenderPass,				//record one render pass into a command buffer
			std::vector<VESubrender *> subRenderers,
			VkFramebuffer *pFrameBuffer,
			uint32_t imageIndex, uint32_t numPass,
			VECamera *pCamera, VELight *pLight);

		///\returns the command pool for this thread - each threads needs its own pool
		virtual VkCommandPool getThreadCommandPool() { return m_commandPools[getEnginePointer()->getThreadPool()->threadNum[std::this_thread::get_id()]]; };
		virtual void updateCmdBuffers() { deleteCmdBuffers(); };
		virtual void deleteCmdBuffers();
		VETexture *m_depthMap = nullptr;				///<the image depth map	
		std::vector<VkSemaphore>	m_imageAvailableSemaphores;			///<sem for waiting for the next swapchain image
		std::vector<VkSemaphore>	m_renderFinishedSemaphores;			///<sem for signalling that rendering done
		std::vector<VkSemaphore>	m_overlaySemaphores;				///<sem for signalling that rendering done
		std::vector<VkFence>		m_inFlightFences;					///<fences for halting the next image render until this one is done
		size_t						m_currentFrame = 0;					///<int for the fences
		bool						m_framebufferResized = false;		///<signal that window size is changing

		std::vector<VkCommandPool>  m_commandPools = {};				///<Array of command pools so that each thread in the thread pool has its own pool
		std::vector<VkCommandBuffer>m_commandBuffers = {};				///<the main command buffers for recording draw commands

		VESubrenderRayTracingKHR_DN *m_subrenderRT = nullptr;	            ///<Pointer to the overlay subrenderer
		VkRenderPass				m_renderPassClear;					///<The first light render pass, clearing the framebuffers
		VkRenderPass				m_renderPassLoad;					///<The second light render pass - no clearing of framebuffer

		VkBuffer m_instancesBuffer;
		VmaAllocation m_instancesBufferAllocation;

		std::vector<std::vector<std::future<secondaryCmdBuf_t>> > m_secondaryBuffersFutures = {};	///<secondary buffers for parallel recording
		std::vector<std::vector<secondaryCmdBuf_t>> m_secondaryBuffers = {};	///<secondary buffers for parallel recording
		AccelerationStructure m_topLevelAS;
		std::vector<AccelerationStructure> m_bottomLevelAS;

		VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_raytracingProperties = {};
		VkPhysicalDeviceAccelerationStructureFeaturesKHR m_raytracingFeatures = {};
	};
}
#endif // VERENDERER_NV_RT_H