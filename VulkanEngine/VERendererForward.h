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

const uint32_t NUM_SHADOW_CASCADE = 4;

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

	public:

		///Data that is updated once per frame
		struct vePerFrameData_t {
			VECamera::veCameraData_t camera;		///<camera data
			VELight::veLightData_t light;			///<Light data
			VECamera::veShadowData_t shadow[NUM_SHADOW_CASCADE]; ///<Shadow data
		};


	protected:
		VETexture *m_depthMap = nullptr;								///<the image depth map	
		std::vector<std::vector<VETexture *>>m_shadowMaps;				///<the shadow maps - a list of map cascades
		std::vector<VkBuffer> m_uniformBuffersPerFrame;					///<UBO for camera, light data and shadow matrices
		std::vector<VmaAllocation> m_uniformBuffersPerFrameAllocation;	///<VMA

		//per frame render resources
		VkRenderPass m_renderPass;										///<The light render pass 
		std::vector<VkFramebuffer> m_swapChainFramebuffers;				///<Framebuffers for light pass

		VkRenderPass m_renderPassShadow;								///<The shadow render pass 
		std::vector<std::vector<VkFramebuffer>> m_shadowFramebuffers;	///<Framebuffers for shadow pass - a list of cascades

		VkDescriptorPool m_descriptorPool;								///<Descriptor pool for creating per frame descriptor sets
		VkDescriptorSetLayout m_descriptorSetLayoutPerFrame;			///<Descriptor set 1: per frame 
		std::vector<VkDescriptorSet> m_descriptorSetsPerFrame;			///<Per frame descriptor sets for set 1
		VkDescriptorSetLayout m_descriptorSetLayoutShadow;				///<Descriptor set 2: shadow
		std::vector<VkDescriptorSet> m_descriptorSetsShadow;			///<Per frame descriptor sets for set 2

		std::vector<VkSemaphore> m_imageAvailableSemaphores;	///<sem for waiting for the next swapchain image
		std::vector<VkSemaphore> m_renderFinishedSemaphores;	///<sem for signalling that rendering done
		std::vector<VkFence> m_inFlightFences;				///<fences for halting the next image render until this one is done
		size_t m_currentFrame = 0;							///<int for the fences
		bool m_framebufferResized = false;					///<signal that window size is changing

		void createSyncObjects();							//create the sync objects
		void cleanupSwapChain();							//delete the swapchain
		void updatePerFrameUBO(uint32_t currentImage);		//update the per frame data like view, proj, lights, shadow

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
		///<\returns the shadow descriptor set layout
		virtual VkDescriptorSetLayout	getDescriptorSetLayoutShadow() { return m_descriptorSetLayoutShadow; };
		///\returns the per frame descriptor set
		virtual std::vector<VkDescriptorSet> &getDescriptorSetsShadow() { return m_descriptorSetsShadow; };

		///\returns the descriptor pool of the per frame descriptors
		virtual VkDescriptorPool		getDescriptorPool() { return m_descriptorPool; };
		///\returns the render pass
		virtual VkRenderPass			getRenderPass() { return m_renderPass; };
		///\returns the shadow render pass
		virtual VkRenderPass			getRenderPassShadow() { return m_renderPassShadow; };
		///\returns the depth map vector
		VETexture *						getDepthMap() { return m_depthMap; };
		///\returns a specific depth map from the whole set
		VETexture *						getShadowMap() { return m_shadowMaps[imageIndex][0]; };
		///\returns the 2D extent of the shadow map
		virtual VkExtent2D				getShadowMapExtent() { return m_shadowMaps[imageIndex][0]->m_extent; };

	};

}



