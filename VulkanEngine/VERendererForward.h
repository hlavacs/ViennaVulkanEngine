/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VERENDERERFORWARD_H
#define VERENDERERFORWARD_H



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

	public: 

		///\brief One secondary command buffer and the pool that it came from
		struct secondaryCmdBuf_t {
			VkCommandBuffer buffer;										///<Vulkan cmd buffer handle
			VkCommandPool pool;											///<Vulkan cmd buffer pool handle
			secondaryCmdBuf_t & operator= (const secondaryCmdBuf_t& right) {	///<copy operator
				buffer = right.buffer;
				pool = right.pool;
				return *this;
			};
		};


		///\brief Shadow and light command buffers for one particular light
		struct secondaryBufferLists_t {
			std::vector<secondaryCmdBuf_t> shadowBuffers = {};						///<list of secondary command buffers for the shadow pass
			std::vector<secondaryCmdBuf_t> lightBuffers = {};						///<list of secondary command buffers for the light pass
			std::vector<std::future<secondaryCmdBuf_t>> shadowBufferFutures = {};	///<futures to wait for if the buffers have been created in parallel
			std::vector<std::future<secondaryCmdBuf_t>> lightBufferFutures = {};	///<futures to wait for 
		};

		///\brief Shadow and light command buffers for one particular light
		struct lightBufferLists_t {
			bool	seenThisLight = false;								///<This light has been rendered, so you do not have to remove this cmd buffer list
			std::vector<secondaryBufferLists_t> lightLists;				///<One list for each image in the swap chain
		};

	protected:
		std::vector<VkCommandPool>		m_commandPools = {};				///<Array of command pools so that each thread in the thread pool has its own pool
		std::vector<VkCommandBuffer>	m_commandBuffers = {};				///<the main command buffers for recording draw commands
		std::vector<std::vector<std::future<secondaryCmdBuf_t>> > m_secondaryBuffersFutures = {};	///<secondary buffers for parallel recording
		std::vector<std::vector<secondaryCmdBuf_t>> m_secondaryBuffers = {};	///<secondary buffers for parallel recording

		std::map<VELight*, lightBufferLists_t> m_lightBufferLists;		///<each light has its own command buffer list, one for each image in the swap chain

		//per frame render resources
		VkRenderPass				m_renderPassClear;					///<The first light render pass, clearing the framebuffers
		VkRenderPass				m_renderPassLoad;					///<The second light render pass - no clearing of framebuffer

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

		void recordCmdBuffers2();
		/*secondaryCmdBuf_t recordRenderpass2(VkRenderPass *pRenderPass,
											std::vector<VESubrender*> subRenderers,
											VkFramebuffer *pFrameBuffer,
											uint32_t imageIndex, uint32_t numPass,
											VECamera *pCamera, VELight *pLight,
											std::vector<VkDescriptorSet> descriptorSets);*/

		void prepareRecording();
		void recordSecondaryBuffers();
		void recordSecondaryBuffersForLight( VELight *Light, uint32_t numPass);
		void recordPrimaryBuffers();

		virtual void drawFrame();					//draw one frame
		virtual void prepareOverlay();				//prepare to draw the overlay
		virtual void drawOverlay();					//Draw the overlay (GUI)
		virtual void presentFrame();				//Present the newly drawn frame
		virtual void closeRenderer();				//close the renderer
		virtual void recreateSwapchain();			//new swapchain due to window size change
		virtual secondaryCmdBuf_t recordRenderpass(	VkRenderPass *pRenderPass,				//record one render pass into a command buffer
													std::vector<VESubrender*> subRenderers,
													VkFramebuffer *pFrameBuffer,
													uint32_t imageIndex, uint32_t numPass,
													VECamera *pCamera, VELight *pLight,
													std::vector<VkDescriptorSet> descriptorSetsShadow);

	public:
		float m_AvgCmdShadowTime = 0.0f;			///<Average time for recording shadow maps
		float m_AvgCmdLightTime = 0.0f;				///<Average time for recording light pass
		float m_AvgRecordTime = 0.0f;				///<Average recording time of one command buffer

		///Constructor of class VERendererForward
		VERendererForward();
		///Destructor of class VERendererForward
		virtual ~VERendererForward() {};
		///\returns the command pool for this thread - each threads needs its own pool
		virtual VkCommandPool getThreadCommandPool() { return m_commandPools[getEnginePointer()->getThreadPool()->threadNum[std::this_thread::get_id()]]; };
		///called whenever the scene graph of the scene manager changes
		virtual void updateCmdBuffers() { deleteCmdBuffers(); };
		virtual void deleteCmdBuffers();
		///\returns the per frame descriptor set layout2 (dynamic buffer)
		virtual VkDescriptorSetLayout	getDescriptorSetLayoutPerObject() { return m_descriptorSetLayoutPerObject; };
		///\returns the shadow descriptor set layout for the shadow
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




#endif

