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


namespace ve {


	class VEEngine;

	/**
	*
	* \brief A deferred renderer
	*
	* This renderer creates four buffers per frame (position,normal,albedo,specular). The shadowing applied after the positions were calculated
	*
	*/
	class VERendererDeferred : public VERenderer {
        friend VESubrenderDF_Composer;

	protected:
		std::vector<VkCommandPool>		m_commandPools = {};				///<Array of command pools so that each thread in the thread pool has its own pool
		std::vector<VkCommandBuffer>	m_commandBuffersOffscreen = {};		///<the main command buffers for recording draw commands
		std::vector<VkCommandBuffer>	m_commandBuffersOnscreen = {};		///<the main command buffers for recording draw commands
		std::vector<std::vector<std::future<secondaryCmdBuf_t>> > m_secondaryBuffersFutures = {};	///<secondary buffers for parallel recording
		std::vector<std::vector<secondaryCmdBuf_t>> m_secondaryBuffers = {};	///<secondary buffers for parallel recording

		std::map<VELight *, lightBufferLists_t> m_lightBufferLists;		///<each light has its own command buffer list, one for each image in the swap chain

        //per frame render resources
        VkRenderPass				m_renderPassClear;					///<The first light render pass, clearing the framebuffers
        VkRenderPass				m_renderPassLoad;					///<The second light render pass - no clearing of framebuffer

        std::vector<VkFramebuffer> m_swapChainFramebuffers;				///<Framebuffers for light pass
		VETexture *m_depthMap = nullptr; 					    		///<the image depth map	
        VETexture *m_positionMap = nullptr;
        VETexture *m_normalMap = nullptr;
        VETexture *m_albedoMap = nullptr;
        std::vector<std::vector<VETexture *>>	m_shadowMaps;			///<the shadow maps - a list of map cascades

        VESubrenderDF_Composer * m_subrenderComposer = nullptr;

        //per frame render resources for the shadow pass
        VkRenderPass				 m_renderPassShadow;				///<The shadow render pass
		std::vector<std::vector<VkFramebuffer>> m_shadowFramebuffers;	///<Framebuffers for shadow pass - a list of cascades
        VkDescriptorSetLayout		 m_descriptorSetLayoutShadow;				///<Descriptor set 2: shadow
        std::vector<VkDescriptorSet> m_descriptorSetsShadow;			///<Per frame descriptor sets for set 2

		std::vector<VkSemaphore> m_imageAvailableSemaphores;	///<sem for waiting for the next swapchain image
		std::vector<VkSemaphore> m_offscreenSemaphores;			///<sem for signalling that offscreen rendering done
		std::vector<VkSemaphore> m_renderFinishedSemaphores;	///<sem for signalling that rendering done
        std::vector<VkSemaphore> m_overlaySemaphores;			///<sem for signalling that rendering done
		std::vector<VkFence>     m_inFlightFences;				///<fences for halting the next image render until this one is done
		size_t                   m_currentFrame = 0;			///<int for the fences
		bool                     m_framebufferResized = false;	///<signal that window size is changing

		void createSyncObjects();							//create the sync objects
		void cleanupSwapChain();							//delete the swapchain

		virtual void initRenderer();						//init the renderer
		virtual void createSubrenderers();					//create the subrenderers
		virtual void destroySubrenderers() override;
		virtual void recordCmdBuffers();			        //record the command buffers
		//void recordCmdBuffers2();

		//void prepareRecording();
		//void recordSecondaryBuffers();
		//void recordSecondaryBuffersForLight(VELight *Light, uint32_t numPass);
		//void recordPrimaryBuffers();

		virtual void drawFrame();							//draw one frame
        virtual void prepareOverlay();				        //prepare to draw the overlay
        virtual void drawOverlay();					        //Draw the overlay (GUI)
		virtual void presentFrame();						//Present the newly drawn frame
		virtual void closeRenderer();						//close the renderer
		virtual void recreateSwapchain();					//new swapchain due to window size change
		virtual secondaryCmdBuf_t recordRenderpass( VkRenderPass *pRenderPass,				//record one render pass into a command buffer
													std::vector<VESubrender *> subRenderers,
													VkFramebuffer *pFrameBuffer,
													uint32_t imageIndex, uint32_t numPass,
													VECamera *pCamera, VELight *pLight,
													std::vector<VkDescriptorSet> descriptorSetsShadow);
	public:
		float m_AvgCmdShadowTime = 0.0f;			///<Average time for recording shadow maps
		float m_AvgCmdGBufferTime = 0.0f;				///<Average time for recording light pass
		float m_AvgCmdLightTime = 0.0f;				///<Average time for recording light pass
		float m_AvgRecordTime = 0.0f;				///<Average recording time of one command buffer

		///Constructor
		VERendererDeferred();
		//Destructor
		virtual ~VERendererDeferred() {};
		virtual VkCommandPool getThreadCommandPool() { return m_commandPools[getEnginePointer()->getThreadPool()->threadNum[std::this_thread::get_id()]]; };
		virtual void updateCmdBuffers() { deleteCmdBuffers(); };
        virtual void deleteCmdBuffers();
		///\returns the shadow descriptor set layout for the shadow
		virtual VkDescriptorSetLayout	getDescriptorSetLayoutShadow() { return m_descriptorSetLayoutShadow; };
		///\returns the per frame descriptor set
		virtual std::vector<VkDescriptorSet> &getDescriptorSetsShadow() { return m_descriptorSetsShadow; };
		///\returns pointer to the swap chain framebuffer vector
		virtual std::vector<VkFramebuffer> &getSwapChainFrameBuffers() { return m_swapChainFramebuffers; };

		///\returns the render pass
		virtual VkRenderPass			getRenderPass() { return m_renderPassClear; };
		///\returns the shadow render pass
		virtual VkRenderPass			getRenderPassShadow() { return m_renderPassShadow; };
		///\returns the depth map vector
		VETexture *getDepthMap() { return m_depthMap; };
		///\returns the position map
		VETexture *getPositionMap() { return m_positionMap; };
		///\returns the normal map
		VETexture *getNormalMap() { return m_normalMap; };
		///\returns the albedo map
		VETexture *getAlbedoMap() { return m_albedoMap; };
		///\returns a specific depth map from the whole set
		std::vector<VETexture *>		getShadowMap(uint32_t idx) { return m_shadowMaps[idx]; };
		///\returns the 2D extent of the shadow map
		virtual VkExtent2D				getShadowMapExtent() { return m_shadowMaps[0][0]->m_extent; };
	};
}



