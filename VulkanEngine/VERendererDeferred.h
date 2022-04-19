/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/

#pragma once

#include "VEInclude.h"

#ifndef getRendererDeferredPointer
#define getRendererDeferredPointer() g_pVERendererDeferredSingleton
#endif

namespace ve
{
	class VEEngine;

	/**
		*
		* \brief A deferred renderer
		*
		* This renderer creates four buffers per frame (position,normal,albedo,specular). The shadowing applied after the positions were calculated
		*
		*/
	class VERendererDeferred : public VERenderer
	{
	protected:
		std::vector<VkCommandPool> m_commandPools = {}; ///<Array of command pools so that each thread in the thread pool has its own pool
		std::vector<VkCommandBuffer> m_commandBuffersOffscreen = {}; ///<the main command buffers for recording draw commands
		std::vector<VkCommandBuffer> m_commandBuffersOnscreen = {}; ///<the main command buffers for recording draw commands
		std::vector<std::vector<secondaryCmdBuf_t>> m_secondaryBuffersOffscreen = {}; ///<secondary buffers for parallel recording
		std::vector<std::vector<std::future<secondaryCmdBuf_t>>>
			m_secondaryBuffersOnscreenFutures = {}; ///<secondary buffers for parallel recording
		std::vector<std::vector<secondaryCmdBuf_t>> m_secondaryBuffersOnscreen = {}; ///<secondary buffers for parallel recording

		std::map<VELight *, lightBufferLists_t> m_lightBufferLists; ///<each light has its own command buffer list, one for each image in the swap chain

		//per frame render resources
		VkRenderPass m_renderPassOnscreenClear;
		VkRenderPass m_renderPassOnscreenLoad;


		VETexture *m_depthMap = nullptr; ///<the image depth map
		std::vector<VETexture *> m_depthMaps;
		std::vector<VETexture *> m_positionMaps;
		std::vector<VETexture *> m_normalMaps;
		std::vector<VETexture *> m_albedoMaps;
		std::vector<std::vector<VETexture *>> m_shadowMaps; ///<the shadow maps - a list of map cascades

		//per frame render resources for the offscreen pass
		VkRenderPass m_renderPassOffscreen;
		std::vector<VkFramebuffer> m_offscreenFramebuffers; ///<Framebuffers for offscreen pass
		VkDescriptorSetLayout m_descriptorSetLayoutOffscreen;
		std::vector<VkDescriptorSet> m_descriptorSetsOffscreen;

		//per frame render resources for the shadow pass
		VkRenderPass m_renderPassShadow; ///<The shadow render pass
		std::vector<std::vector<VkFramebuffer>> m_shadowFramebuffers; ///<Framebuffers for shadow pass - a list of cascades
		VkDescriptorSetLayout m_descriptorSetLayoutShadow; ///<Descriptor set 2: shadow
		std::vector<VkDescriptorSet> m_descriptorSetsShadow; ///<Per frame descriptor sets for set 2

		std::vector<VkSemaphore> m_imageAvailableSemaphores; ///<sem for waiting for the next swapchain image
		std::vector<VkSemaphore> m_offscreenSemaphores; ///<sem for signalling that offscreen rendering done
		std::vector<VkSemaphore> m_renderFinishedSemaphores; ///<sem for signalling that rendering done
		std::vector<VkSemaphore> m_overlaySemaphores; ///<sem for signalling that rendering done
		std::vector<VkFence> m_inFlightFences; ///<fences for halting the next image render until this one is done
		size_t m_currentFrame = 0; ///<int for the fences
		bool m_framebufferResized = false; ///<signal that window size is changing

		void createSyncObjects(); //create the sync objects
		void cleanupSwapChain(); //delete the swapchain

		virtual void initRenderer(); //init the renderer
		virtual void createSubrenderers(); //create the subrenderers

		virtual void recordCmdBuffersOffscreen(); //record the command buffers
		virtual void recordCmdBuffersOnscreen(); //record the command buffers

		virtual void drawFrame(); //draw one frame
		virtual void prepareOverlay(); //prepare to draw the overlay
		virtual void drawOverlay(); //Draw the overlay (GUI)
		virtual void presentFrame(); //Present the newly drawn frame
		virtual void closeRenderer(); //close the renderer
		virtual void recreateSwapchain(); //new swapchain due to window size change
		virtual secondaryCmdBuf_t
			recordRenderpass(VkRenderPass *pRenderPass, //record one render pass into a command buffer
				std::vector<VESubrender *> subRenderers,
				VkFramebuffer *pFrameBuffer,
				uint32_t imageIndex,
				uint32_t numPass,
				VECamera *pCamera,
				VELight *pLight,
				std::vector<VkDescriptorSet> descriptorSetsShadow);

	public:
		///Constructor
		VERendererDeferred();

		//Destructor
		virtual ~VERendererDeferred() {};

		virtual VkCommandPool
			getThreadCommandPool()
		{
			return m_commandPools[getEnginePointer()->getThreadPool()->threadNum[std::this_thread::get_id()]];
		};

		virtual void updateCmdBuffers()
		{
			deleteCmdBuffers();
		};

		virtual void deleteCmdBuffers();

		///\returns the shadow descriptor set layout for the shadow
		virtual VkDescriptorSetLayout getDescriptorSetLayoutShadow()
		{
			return m_descriptorSetLayoutShadow;
		};

		virtual VkDescriptorSetLayout getDescriptorSetLayoutOffscreen()
		{
			return m_descriptorSetLayoutOffscreen;
		};

		///\returns the per frame descriptor set
		virtual std::vector<VkDescriptorSet> &getDescriptorSetsShadow()
		{
			return m_descriptorSetsShadow;
		};

		virtual std::vector<VkDescriptorSet> &getDescriptorSetsOffscreen()
		{
			return m_descriptorSetsOffscreen;
		};

		///\returns the offscreen render pass
		virtual VkRenderPass getRenderPassOffscreen()
		{
			return m_renderPassOffscreen;
		};

		///\returns the onscreen render pass
		virtual VkRenderPass getRenderPassOnscreen()
		{
			return m_renderPassOnscreenClear;
		};

		///\returns the shadow render pass
		virtual VkRenderPass getRenderPassShadow()
		{
			return m_renderPassShadow;
		};

		///\returns the depth map vector
		VETexture *getDepthMap()
		{
			return m_depthMap;
		};

		///\returns a specific depth map from the whole set
		std::vector<VETexture *> getShadowMap(uint32_t idx)
		{
			return m_shadowMaps[idx];
		};

		///\returns the 2D extent of the shadow map
		virtual VkExtent2D getShadowMapExtent()
		{
			return m_shadowMaps[0][0]->m_extent;
		};
	};
} // namespace ve
