#pragma once


namespace vh {

    void DevCreateInstance(const std::vector<const char*>& validationLayers, 
		const std::vector<const char *>& extensions, const std::string& name, 
		uint32_t apiVersion, bool debug, VkInstance &instance);
	
	VkResult DevCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	
	void DevDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger, const VkAllocationCallbacks* pAllocator);
    
	void DevInitVMA(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, uint32_t apiVersion, VmaAllocator& allocator);
    
	void DevCleanupSwapChain(VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage);
    
	void DevRecreateSwapChain(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage, VkRenderPass renderPass);
    
	void DevPopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    
	void DevSetupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger);
    
    VKAPI_ATTR VkBool32 VKAPI_CALL DevDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity
        , VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData
        , void* pUserData);

	void DevCreateSurface(VkInstance instance, SDL_Window *sdlWindow, VkSurfaceKHR& surface);
    
	void DevPickPhysicalDevice(VkInstance instance, const std::vector<const char*>& deviceExtensions, VkSurfaceKHR surface, VkPhysicalDevice& physicalDevice);

    void DevCreateLogicalDevice(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, QueueFamilyIndices& queueFamilies, 
		const std::vector<const char*>& validationLayers, const std::vector<const char*>& deviceExtensions, bool debug, 
		VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue);

    void DevCreateSwapChain(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain);

    void DevCreateImageViews(VkDevice device, SwapChain& swapChain);

	VkSurfaceFormatKHR DevChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR DevChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D DevChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* sdlWindow);

    SwapChainSupportDetails DevQuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    bool DevIsDeviceSuitable(VkPhysicalDevice device, const std::vector<const char *>&extensions , VkSurfaceKHR surface);

    bool DevCheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions);

    QueueFamilyIndices DevFindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

    bool DevCheckValidationLayerSupport(const std::vector<const char*>& validationLayers);

} // namespace vh
