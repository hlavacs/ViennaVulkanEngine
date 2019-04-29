/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#pragma once

#ifndef getRendererForwardPointer
#define getRendererForwardPointer() g_pVERendererForwardSingleton
#endif

const uint32_t NUM_SHADOW_CASCADE = 6;

namespace ve {


	extern VERendererForward* g_pVERendererForwardSingleton;	///<Pointer to the only class instance 

	class VEEngine;

	/**
	*
	* \brief A classical forward renderer
	*
	* This renderer first clears the framebuffer, then starts rendering each entity one by one.
	*
	*/
	class VERendererForward : public VERenderer {

	protected:
		//per frame render resources
		VkRenderPass				m_renderPassClear;					///<The first light render pass, clearing the framebuffers
		VkRenderPass				m_renderPassLoad;					///<The second light render pass

		std::vector<VkFramebuffer>	m_swapChainFramebuffers;			///<Framebuffers for light pass
		VETexture *					m_depthMap = nullptr;				///<the image depth map	
		std::vector<std::vector<VETexture *>>	m_shadowMaps;			///<the shadow maps - a list of map cascades

		//per frame render resources for the shadow pass
		VkRenderPass				 m_renderPassShadow;				///<The shadow render pass 
		std::vector<std::vector<VkFramebuffer>>	 m_shadowFramebuffers;	///<list of Framebuffers for shadow pass (up to 6)
		VkDescriptorSetLayout		 m_descriptorSetLayoutShadow;		///<Descriptor set layout for using shadow maps in the light pass
		std::vector<VkDescriptorSet> m_descriptorSetsShadow;			///<Descriptor sets for usage of shadow maps in the light pass

		VkDescriptorPool			m_descriptorPool;					///<Descriptor pool for creating descriptor sets
		VkDescriptorSetLayout		m_descriptorSetLayoutPerObject;		///<Descriptor set layout for each scene object

		std::vector<VkSemaphore>	m_imageAvailableSemaphores;			///<sem for waiting for the next swapchain image
		std::vector<VkSemaphore>	m_renderFinishedSemaphores;			///<sem for signalling that rendering done
		std::vector<VkSemaphore>	m_overlaySemaphores;				///<sem for signalling that rendering done
		std::vector<VkFence>		m_inFlightFences;					///<fences for halting the next image render until this one is done
		size_t						m_currentFrame = 0;					///<int for the fences
		bool						m_framebufferResized = false;		///<signal that window size is changing

		void createSyncObjects();					//create the sync objects
		void cleanupSwapChain();					//delete the swapchain

		virtual void initRenderer();				//init the renderer
		virtual void createSubrenderers();			//create the subrenderers
		virtual void recordCmdBuffers();			//record the command buffers
		virtual void drawFrame();					//draw one frame
		virtual void prepareOverlay();				//prepare to draw the overlay
		virtual void drawOverlay();					//Draw the overlay (GUI)
		virtual void presentFrame();				//Present the newly drawn frame
		virtual void closeRenderer();				//close the renderer
		virtual void recreateSwapchain();			//new swapchain due to window size change

	public:
		float m_AvgCmdShadowTime = 0.0f;
		float m_AvgCmdLightTime = 0.0f;
		float m_AvgCmdCommitTime = 0.0f;

		///Constructor
		VERendererForward();
		//Destructor
		virtual ~VERendererForward() {};
		///<\returns the per frame descriptor set layout
		virtual VkDescriptorSetLayout	getDescriptorSetLayoutPerObject() { return m_descriptorSetLayoutPerObject; };
		///<\returns the shadow descriptor set layout
		virtual VkDescriptorSetLayout	getDescriptorSetLayoutShadow() { return m_descriptorSetLayoutShadow; };
		///\returns the per frame descriptor set
		virtual std::vector<VkDescriptorSet> &getDescriptorSetsShadow() { return m_descriptorSetsShadow; };
		///\returns pointer to the swap chain framebuffer vector
		virtual std::vector<VkFramebuffer> &getSwapChainFrameBuffers() { return m_swapChainFramebuffers;  };
		///\returns the descriptor pool of the per frame descriptors
		virtual VkDescriptorPool		getDescriptorPool() { return m_descriptorPool; };
		///\returns the render pass
		virtual VkRenderPass			getRenderPass() { return m_renderPassClear; };
		///\returns the shadow render pass
		virtual VkRenderPass			getRenderPassShadow() { return m_renderPassShadow; };
		///\returns the depth map vector
		VETexture *						getDepthMap() { return m_depthMap; };
		///\returns a specific depth map from the whole set
		std::vector<VETexture *>		getShadowMap( uint32_t idx) { return m_shadowMaps[idx]; };
		///\returns the 2D extent of the shadow map
		virtual VkExtent2D				getShadowMapExtent() { return m_shadowMaps[0][0]->m_extent; };
	};

}



