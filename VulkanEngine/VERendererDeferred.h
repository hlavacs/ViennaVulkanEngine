/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/

#pragma once


#include "VEInclude.h"
#include "VESubrenderFW_Nuklear.h"

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
        std::vector<VkCommandBuffer> m_commandBuffers = {};				///<the main command buffers for recording draw commands

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
        VESubrenderFW_Nuklear * m_subrenderNuklear = nullptr;

        //per frame render resources for the shadow pass
        VkRenderPass				 m_renderPassShadow;				///<The shadow render pass
		std::vector<std::vector<VkFramebuffer>> m_shadowFramebuffers;	///<Framebuffers for shadow pass - a list of cascades
        VkDescriptorSetLayout m_descriptorSetLayoutShadow;				///<Descriptor set 2: shadow
        std::vector<VkDescriptorSet> m_descriptorSetsShadow;			///<Per frame descriptor sets for set 2

		VkDescriptorPool            m_descriptorPool;					///<Descriptor pool for creating per frame descriptor sets
        VkDescriptorSetLayout		m_descriptorSetLayoutPerObject;		///<Descriptor set layout for each scene object


		std::vector<VkSemaphore> m_imageAvailableSemaphores;	///<sem for waiting for the next swapchain image
		std::vector<VkSemaphore> m_renderFinishedSemaphores;	///<sem for signalling that rendering done
        std::vector<VkSemaphore> m_overlaySemaphores;			///<sem for signalling that rendering done
		std::vector<VkFence>     m_inFlightFences;				///<fences for halting the next image render until this one is done
		size_t                   m_currentFrame = 0;			///<int for the fences
		bool                     m_framebufferResized = false;	///<signal that window size is changing

		void createSyncObjects();							//create the sync objects
		void cleanupSwapChain();							//delete the swapchain

		virtual void initRenderer();						//init the renderer
		virtual void createSubrenderers();					//create the subrenderers
        virtual void recordCmdBuffers();			        //record the command buffers
		virtual void drawFrame();							//draw one frame
        virtual void prepareOverlay();				        //prepare to draw the overlay
        virtual void drawOverlay();					        //Draw the overlay (GUI)
		virtual void presentFrame();						//Present the newly drawn frame
		virtual void closeRenderer();						//close the renderer
		virtual void recreateSwapchain();					//new swapchain due to window size change
        void destroySubrenderers() override;
	public:
        float m_AvgCmdShadowTime = 0.0f;			///<Average time for recording shadow maps
        float m_AvgCmdLightTime = 0.0f;				///<Average time for recording light pass

		///Constructor
		VERendererDeferred();
		//Destructor
		virtual ~VERendererDeferred() {};
        virtual void deleteCmdBuffers();
        ///\returns the per frame descriptor set layout
        virtual VkDescriptorSetLayout	getDescriptorSetLayoutPerObject() { return m_descriptorSetLayoutPerObject; };
		///<\returns the shadow descriptor set layout
		virtual VkDescriptorSetLayout	getDescriptorSetLayoutShadow() { return m_descriptorSetLayoutShadow; };
		///\returns the per frame descriptor set
		virtual std::vector<VkDescriptorSet> &getDescriptorSetsShadow() { return m_descriptorSetsShadow; };
        ///\returns pointer to the swap chain framebuffer vector
        virtual std::vector<VkFramebuffer> &getSwapChainFrameBuffers() { return m_swapChainFramebuffers; };
		///\returns the descriptor pool of the per frame descriptors
		virtual VkDescriptorPool		getDescriptorPool() { return m_descriptorPool; };
		///\returns the render pass
		virtual VkRenderPass			getRenderPass() { return m_renderPassClear; };
		///\returns the depth map vector
		VETexture *						getDepthMap() { return m_depthMap; };
        ///\returns
        VETexture *						getPositionMap() { return m_positionMap; };
        ///\returns
        VETexture *						getNormalMap() { return m_normalMap; };
        ///\returns
        VETexture *						getAlbedoMap() { return m_albedoMap; };
		///\returns the shadow render pass
		virtual VkRenderPass			getRenderPassShadow() { return m_renderPassShadow; };
        ///\returns a specific depth map from the whole set
        std::vector<VETexture *>		getShadowMap(uint32_t idx) { return m_shadowMaps[idx]; };
		///\returns the 2D extent of the shadow map
		virtual VkExtent2D				getShadowMapExtent() { return m_shadowMaps[0][0]->m_extent; };
	};


	extern VERendererDeferred* g_pVERendererDeferredSingleton;	///<Pointer to the only class instance 
}



