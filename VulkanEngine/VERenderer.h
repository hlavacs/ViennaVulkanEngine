/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VERENDERER_H
#define VERENDERER_H

namespace ve
{
	class VEEngine;

	class VESceneManager;

	/**
		*
		* \brief Base class of all renderers
		*
		* A VERenderer is responsible for rendering a frame. It creates resources for this, and manages the whole
		* rendering process. However, it does not directly call vkDraw(...). Instead for this it creates and uses
		* a list of VESubrender classes that are responsible for handling different entity types.
		*
		*/
	class VERenderer
	{
		friend VEEngine;
		friend VESceneManager;
		friend VESubrender;

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

		///\brief Shadow and light command buffers for one particular light
		struct secondaryBufferLists_t
		{
			std::vector<secondaryCmdBuf_t> shadowBuffers = {}; ///<list of secondary command buffers for the shadow pass
			std::vector<secondaryCmdBuf_t> lightBuffers = {}; ///<list of secondary command buffers for the light pass
			std::vector<std::future<secondaryCmdBuf_t>> shadowBufferFutures = {}; ///<futures to wait for if the buffers have been created in parallel
			std::vector<std::future<secondaryCmdBuf_t>> lightBufferFutures = {}; ///<futures to wait for
		};

		///\brief Shadow and light command buffers for one particular light
		struct lightBufferLists_t
		{
			bool seenThisLight = false; ///<This light has been rendered, so you do not have to remove this cmd buffer list
			std::vector<secondaryBufferLists_t> lightLists; ///<One list for each image in the swap chain
		};

	protected:
		VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE; ///<Vulkan physical device handle
		VkPhysicalDeviceFeatures m_deviceFeatures = {}; ///<Features of the physical device
		VkPhysicalDeviceLimits m_deviceLimits = {}; ///<Limits of the physical device

		VkDevice m_device; ///<Vulkan logical device handle
		VkQueue m_graphicsQueue; ///<Vulkan graphics queue
		VkQueue m_presentQueue; ///<Vulkan present queue
		VmaAllocator m_vmaAllocator; ///<VMA allocator
		VkCommandPool m_commandPool; ///<Command pool of this thread

		//surface
		VkSurfaceKHR m_surface; ///<Vulkan KHR surface
		VkSurfaceCapabilitiesKHR m_surfaceCapabilities; ///<Surface capabilities
		VkSurfaceFormatKHR m_surfaceFormat; ///<Surface format

		//swapchain
		VkSwapchainKHR m_swapChain; ///<Vulkan KHR swapchain
		std::vector<VkFramebuffer> m_swapChainFramebuffers; ///<Framebuffers for light pass
		std::vector<VkImage> m_swapChainImages; ///<A list of the swap chain images
		VkFormat m_swapChainImageFormat; ///<Swap chain image format
		VkExtent2D m_swapChainExtent; ///<Image extent of the swap chain images
		std::vector<VkImageView> m_swapChainImageViews; ///<Image views of the swap chain images
		uint32_t m_imageIndex = 0; ///<Index of the current swapchain image

		VkDescriptorPool m_descriptorPool; ///<Descriptor pool for creating descriptor sets
		VkDescriptorSetLayout m_descriptorSetLayoutPerObject; ///<Descriptor set layout for each scene object

		//subrenderers
		std::vector<VESubrender *> m_subrenderers; ///<Subrenderers for lit objects
		VESubrender *m_subrenderShadow = nullptr; ///<Pointer to the shadow subrenderer
		VESubrender *m_subrenderOverlay = nullptr; ///<Pointer to the overlay subrenderer
		VESubrender *m_subrenderRT = nullptr; ///<Pointer to the raytracing subrenderer
		VESubrender *m_subrenderComposer = nullptr; ///<Pointer to the composer subrenderer (Deferred rendering)

		///Initialize the base class
		virtual void initRenderer() {};

		///Close the base class
		virtual void closeRenderer() {};

		///Base class does not create subrennderers directly
		virtual void createSubrenderers() {};

		virtual void addSubrenderer(VESubrender *pSub);

		virtual VESubrender *getSubrenderer(veSubrenderType type);

		virtual void destroySubrenderers();

		///Draw one frame
		virtual void drawFrame() {};

		///Draw the overlay (GUI)
		virtual void prepareOverlay() {};

		///Draw the overlay (GUI)
		virtual void drawOverlay() {};

		///Present the newl drawn frame
		virtual void presentFrame() {};

		///Recreate the swap chain
		virtual void recreateSwapchain() {};

	public:
		float m_AvgCmdShadowTime = 0.0f; ///<Average time for recording shadow maps
		float m_AvgCmdLightTime = 0.0f; ///<Average time for recording light pass
		float m_AvgCmdGBufferTime = 0.0f; ///<Average time for recording light pass
		float m_AvgRecordTimeOffscreen = 0.0f; ///<Average recording time of one offscreen command buffer
		float m_AvgRecordTimeOnscreen = 0.0f; ///<Average recording time of one onscreen command buffer
		///Constructor
		VERenderer();

		///Destructor
		virtual ~VERenderer() {};

		///this function is calledby the scene manager if the tree changed
		virtual void updateCmdBuffers() {};

		///\returns the VMA allocator
		virtual VmaAllocator getVmaAllocator()
		{
			return m_vmaAllocator;
		};

		///\returns the Vulkan physical device
		virtual VkPhysicalDevice getPhysicalDevice()
		{
			return m_physicalDevice;
		};

		///\returns the Vulkan logical device
		virtual VkDevice getDevice()
		{
			return m_device;
		};

		///\returns the per frame descriptor set layout2 (dynamic buffer)
		virtual VkDescriptorSetLayout getDescriptorSetLayoutPerObject()
		{
			return m_descriptorSetLayoutPerObject;
		};

		///\returns the descriptor pool of the per frame descriptors
		virtual VkDescriptorPool getDescriptorPool()
		{
			return m_descriptorPool;
		};

		///\returns the KHR surface that connects the window to Vulkan
		virtual VkSurfaceKHR getSurface()
		{
			return m_surface;
		};

		///\returns the Vulkan graphics queue
		virtual VkQueue getGraphicsQueue()
		{
			return m_graphicsQueue;
		};

		///\returns the thread command pool
		virtual VkCommandPool getCommandPool()
		{
			return m_commandPool;
		};
		
		///\returns pointer to the swap chain framebuffer vector
		virtual std::vector<VkFramebuffer> &getSwapChainFrameBuffers()
		{
			return m_swapChainFramebuffers;
		};

		///\returns the swap chain image format
		virtual VkFormat getSwapChainImageFormat()
		{
			return m_swapChainImageFormat;
		};

		///\returns the swap chain image extent
		virtual VkExtent2D getSwapChainExtent()
		{
			return m_swapChainExtent;
		};

		///\returns the number of swap chain images
		virtual uint32_t getSwapChainNumber()
		{
			return (uint32_t)m_swapChainImages.size();
		};

		///\returns the index of the swap chain image that is current prepared for drawing
		virtual uint32_t getImageIndex()
		{
			return m_imageIndex;
		};

		///\returns the current swap chain image
		virtual VkImage getSwapChainImage()
		{
			return m_swapChainImages[m_imageIndex];
		};

		///\returns the overlay (GUI) subrenderer
		virtual VESubrender *getOverlay()
		{
			return m_subrenderOverlay;
		};

		virtual void addEntityToSubrenderer(VEEntity *pEntity);

		virtual void removeEntityFromSubrenderers(VEEntity *pEntity);

		///\returns the depth map vector
		virtual VETexture *getDepthMap() = 0;
	};

} // namespace ve

#endif
