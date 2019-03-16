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
		VkImage m_depthImage;											///<Depth image 
		VmaAllocation m_depthImageAllocation;							///<VMA image allocation info
		VkImageView m_depthImageView;									///<Image view for depth image
		VkFormat m_depthMapFormat;										///<remember the depth map format

		VkImage m_shadowMap;											///<shadow map image 
		VmaAllocation m_shadowMapAllocation;							///<Shadow map VMA image allocation info
		VkImageView m_shadowMapView;									///<Image view for shadow map
		VkExtent2D  m_shadowMapExtent;									///<extent of shadow map

		std::vector<VkBuffer> m_uniformBuffersPerFrame;					///<UBO for camera and light data
		std::vector<VmaAllocation> m_uniformBuffersPerFrameAllocation;	///<VMA

		//per frame render resources
		VkRenderPass m_renderPass;								///<The light render pass 
		std::vector<VkFramebuffer> m_swapChainFramebuffers;		///<Framebuffers for light pass
		VkRenderPass m_renderPassShadow;						///<The shadow render pass 
		std::vector<VkFramebuffer> m_shadowFramebuffers;		///<Framebuffers for shadow pass
		VkDescriptorPool m_descriptorPool;						///<Descriptor pool for creating per frame descriptor sets
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;	///<Descriptor set per frame layout
		std::vector<VkDescriptorSet> m_descriptorSetsPerFrame;	///<Per frame descriptor sets

		std::vector<VkSemaphore> m_imageAvailableSemaphores;	///<sem for waiting for the next swapchain image
		std::vector<VkSemaphore> m_renderFinishedSemaphores;	///<sem for signalling that rendering done
		std::vector<VkFence> m_inFlightFences;				///<fences for halting the next image render until this one is done
		size_t m_currentFrame = 0;							///<int for the fences
		bool m_framebufferResized = false;					///<signal that window size is changing

		void createSyncObjects();							//create the sync objects
		void cleanupSwapChain();							//delete the swapchain
		void updatePerFrameUBO(uint32_t currentImage);		//update the per frame data like view, proj, lights

		virtual void initRenderer();						//init the renderer
		virtual void createSubrenderers();					//create the subrenderers
		virtual void drawFrame();							//draw one frame
		virtual void presentFrame();						//Present the newly drawn frame
		virtual void closeRenderer();						//close the renderer
		virtual void recreateSwapchain();					//new swapchain due to window size change

	public:
		///Constructor
		VERendererForward();
		//Destructor
		virtual ~VERendererForward() {};

		///<\returns the per frame descriptor set layout
		virtual VkDescriptorSetLayout	getDescriptorSetLayoutPerFrame() { return m_descriptorSetLayoutPerFrame; };
		///\returns the per frame descriptor set
		virtual std::vector<VkDescriptorSet> &getDescriptorSetsPerFrame() { return m_descriptorSetsPerFrame; };
		///\returns the descriptor pool of the per frame descriptors
		virtual VkDescriptorPool		getDescriptorPool() { return m_descriptorPool; };
		///\returns the render pass
		virtual VkRenderPass			getRenderPass() { return m_renderPass; };
		///\returns the shadow render pass
		virtual VkRenderPass			getRenderPassShadow() { return m_renderPassShadow; };
		///\returns the 2D extent of the shadow map
		virtual VkExtent2D				getShadowMapExtent() { return m_shadowMapExtent; };

	};

}



