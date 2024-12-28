#pragma once


namespace vh {

    void createInstance(const std::vector<const char*>& validationLayers, const std::vector<const char *>& extensions, VkInstance &instance);
	
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger, const VkAllocationCallbacks* pAllocator);
    
	void initVMA(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator& allocator);
    
	void cleanupSwapChain(VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage);
    
	void recreateSwapChain(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, SwapChain& swapChain, DepthImage& depthImage, VkRenderPass renderPass);
    
	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    
	void setupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger);
    
	void createSurface(VkInstance instance, SDL_Window *sdlWindow, VkSurfaceKHR& surface);
    
	void pickPhysicalDevice(VkInstance instance, const std::vector<const char*>& deviceExtensions, VkSurfaceKHR surface, VkPhysicalDevice& physicalDevice);

    void createLogicalDevice(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, QueueFamilyIndices& queueFamilies, 
		const std::vector<const char*>& validationLayers, const std::vector<const char*>& deviceExtensions, 
		VkDevice& device, VkQueue& graphicsQueue, VkQueue& presentQueue);

    void createSwapChain(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain);

} // namespace vh
