#pragma once


namespace vh {

    void createInstance(const std::vector<const char*>& validationLayers, 
		const std::vector<const char *>& extensions, bool debug, VkInstance &instance);
	
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger, const VkAllocationCallbacks* pAllocator);
    
	void initVMA(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator& allocator);
    
	void cleanupSwapChain(VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage);
    
	void recreateSwapChain(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage, VkRenderPass renderPass);
    
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    
	void setupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger);
    
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity
        , VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData
        , void* pUserData);

	void createSurface(VkInstance instance, SDL_Window *sdlWindow, VkSurfaceKHR& surface);
    
	void pickPhysicalDevice(VkInstance instance, const std::vector<const char*>& deviceExtensions, VkSurfaceKHR surface, VkPhysicalDevice& physicalDevice);

    void createLogicalDevice(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, QueueFamilyIndices& queueFamilies, 
		const std::vector<const char*>& validationLayers, const std::vector<const char*>& deviceExtensions, bool debug, 
		VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue);

    void createSwapChain(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain);

    void createImageViews(VkDevice device, SwapChain& swapChain);


	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* sdlWindow);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    bool isDeviceSuitable(VkPhysicalDevice device, const std::vector<const char *>&extensions , VkSurfaceKHR surface);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers);

} // namespace vh
