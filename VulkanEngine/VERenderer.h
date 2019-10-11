/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VERENDERER_H
#define VERENDERER_H


#ifndef getRendererPointer
#define getRendererPointer() g_pVERendererSingleton
#endif

namespace ve {
	class VEEngine;
	class VESceneManager;

	extern VERenderer* g_pVERendererSingleton;	///<Pointer to the only class instance 

	/**
	*
	* \brief Base class of all renderers
	*
	* A VERenderer is responsible for rendering a frame. It creates resources for this, and manages the whole
	* rendering process. However, it does not directly call vkDraw(...). Instead for this it creates and uses 
	* a list of VESubrender classes that are responsible for handling different entity types.
	*
	*/
	class VERenderer {
		friend VEEngine;
		friend VESceneManager;
		friend VESubrender;

	protected:
		VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;		///<Vulkan physical device handle
		VkPhysicalDeviceFeatures m_deviceFeatures = {};			///<Features of the physical device
		VkPhysicalDeviceLimits m_deviceLimits = {};				///<Limits of the physical device

		VkDevice m_device;										///<Vulkan logical device handle
		VkQueue m_graphicsQueue;								///<Vulkan graphics queue
		VkQueue m_presentQueue;									///<Vulkan present queue
		VmaAllocator m_vmaAllocator;							///<VMA allocator
		VkCommandPool m_commandPool;							///<Command pool of this thread

		//surface
		VkSurfaceKHR m_surface;									///<Vulkan KHR surface
		VkSurfaceCapabilitiesKHR m_surfaceCapabilities;			///<Surface capabilities
		VkSurfaceFormatKHR m_surfaceFormat;						///<Surface format

		//swapchain
		VkSwapchainKHR m_swapChain;								///<Vulkan KHR swapchain
		std::vector<VkImage> m_swapChainImages;					///<A list of the swap chain images
		VkFormat m_swapChainImageFormat;						///<Swap chain image format
		VkExtent2D m_swapChainExtent;							///<Image extent of the swap chain images
		std::vector<VkImageView> m_swapChainImageViews;			///<Image views of the swap chain images
		uint32_t m_imageIndex = 0;								///<Index of the current swapchain image

		//subrenderers
		std::vector<VESubrender*> m_subrenderers;				///<Subrenderers for lit objects
		VESubrender *			  m_subrenderShadow=nullptr;	///<Pointer to the shadow subrenderer
		VESubrender *			  m_subrenderOverlay = nullptr;	///<Pointer to the overlay subrenderer

		///Initialize the base class
		virtual void initRenderer() {};
		///Close the base class
		virtual void closeRenderer() {};
		///Base class does not create subrennderers directly
		virtual void createSubrenderers() {};
		virtual void addSubrenderer( VESubrender *pSub);
		virtual VESubrender * getSubrenderer( VESubrender::veSubrenderType );
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
		///Constructor
		VERenderer();
		///Destructor
		virtual ~VERenderer() {};
		///this function is calledby the scene manager if the tree changed
		virtual void					updateCmdBuffers() {};
		///\returns the VMA allocator
		virtual VmaAllocator			getVmaAllocator() { return m_vmaAllocator; };
		///\returns the Vulkan physical device
		virtual VkPhysicalDevice		getPhysicalDevice() { return m_physicalDevice; };
		///\returns the Vulkan logical device
		virtual VkDevice				getDevice() { return m_device;  };
		///\returns the KHR surface that connects the window to Vulkan
		virtual VkSurfaceKHR			getSurface() { return m_surface; };
		///\returns the Vulkan graphics queue
		virtual VkQueue					getGraphicsQueue() { return m_graphicsQueue; };
		///\returns the thread command pool
		virtual VkCommandPool			getCommandPool() { return m_commandPool;  };
		///\returns the swap chain image format
		virtual VkFormat				getSwapChainImageFormat() { return m_swapChainImageFormat; };
		///\returns the swap chain image extent
		virtual VkExtent2D				getSwapChainExtent() { return m_swapChainExtent;  };
		///\returns the number of swap chain images
		virtual uint32_t				getSwapChainNumber() { return (uint32_t)m_swapChainImages.size();  };
		///\returns the index of the swap chain image that is current prepared for drawing
		virtual uint32_t				getImageIndex() { return m_imageIndex;  };
		///\returns the current swap chain image
		virtual VkImage					getSwapChainImage() { return m_swapChainImages[m_imageIndex]; };
		///\returns the overlay (GUI) subrenderer
		virtual VESubrender *			getOverlay() { return m_subrenderOverlay; };
		virtual void					addEntityToSubrenderer(VEEntity *pEntity);
		virtual void					removeEntityFromSubrenderers(VEEntity *pEntity);
	};

}



#endif


