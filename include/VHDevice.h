#pragma once


namespace vvh {

	inline VkInstance volkInstance;

	//---------------------------------------------------------------------------------------------

	inline bool DevCheckValidationLayerSupport(const std::vector<std::string>& validationLayers) {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (auto& layerName : validationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName.c_str(), layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) { return false; }
		}
		return true;
	}
	
	//---------------------------------------------------------------------------------------------

	inline VKAPI_ATTR VkBool32 VKAPI_CALL DevDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity
			, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData
			, void* pUserData) {
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;
	}

	//---------------------------------------------------------------------------------------------

	inline VkResult DevCreateDebugUtilsMessengerEXT(
			VkInstance 							instance,
			VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
			VkAllocationCallbacks* 				pAllocator,
			VkDebugUtilsMessengerEXT* 			pDebugMessenger) {

		auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		} else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	//--------------------------------------------------------------------------------------------- 

	inline void DevPopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
			| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
			| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DevDebugCallback;
	}
	
	//---------------------------------------------------------------------------------------------

	inline void DevSetupDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT& debugMessenger) {
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		DevPopulateDebugMessengerCreateInfo(createInfo);
		if (DevCreateDebugUtilsMessengerEXT( instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}
	
	//---------------------------------------------------------------------------------------------

	struct DevCheckDeviceExtensionSupportInfo {
		const VkPhysicalDevice& 		m_physicalDevice; 
		const std::vector<std::string>& m_deviceExtensions;
	};

	template<typename T = DevCheckDeviceExtensionSupportInfo>
	bool DevCheckDeviceExtensionSupport(T&& info) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(info.m_physicalDevice, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(info.m_physicalDevice, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(info.m_deviceExtensions.begin(), info.m_deviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }


	//---------------------------------------------------------------------------------------------
	struct DevFindQueueFamiliesInfo{
		const VkPhysicalDevice& m_physicalDevice;
		const VkSurfaceKHR& 	m_surface;
	};

	template<typename T = DevFindQueueFamiliesInfo>
    auto DevFindQueueFamilies(T&& info) -> QueueFamilyIndices {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(info.m_physicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(info.m_physicalDevice, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(info.m_physicalDevice, i, info.m_surface, &presentSupport);

            if (presentSupport) { indices.presentFamily = i; }
            if (indices.isComplete()) { break; }
            i++;
        }

        return indices;
    }


	//---------------------------------------------------------------------------------------------

    struct DevQuerySwapChainSupportInfo {
        const VkPhysicalDevice& m_physicalDevice;
		const VkSurfaceKHR& 	m_surface;
	};

	template<typename T = DevQuerySwapChainSupportInfo>
	auto DevQuerySwapChainSupport(T&& info) -> SwapChainSupportDetails {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(info.m_physicalDevice, info.m_surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(info.m_physicalDevice, info.m_surface, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(info.m_physicalDevice, info.m_surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(info.m_physicalDevice, info.m_surface, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(info.m_physicalDevice, info.m_surface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

	//---------------------------------------------------------------------------------------------

	struct DevIsDeviceSuitableInfo {
		const VkPhysicalDevice& 		m_physicalDevice;
		const std::vector<std::string>& m_deviceExtensions; 
		const VkSurfaceKHR& 			m_surface;
	};

	template<typename T = DevIsDeviceSuitableInfo>
    bool DevIsDeviceSuitable(T&& info) {
        QueueFamilyIndices indices = DevFindQueueFamilies(info);

        bool extensionsSupported = DevCheckDeviceExtensionSupport(info);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = DevQuerySwapChainSupport(info);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(info.m_physicalDevice, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate  && supportedFeatures.samplerAnisotropy;
    }


	//---------------------------------------------------------------------------------------------

	struct DevCreateInstanceInfo {
		const std::vector<std::string>& m_validationLayers;
		const std::vector<std::string>& m_instanceExtensions; 
		const std::string& 	m_name; 
		uint32_t& 			m_apiVersion;
		bool& 				m_debug; 
		VkInstance& 		m_instance;
	};

	template<typename T = DevCreateInstanceInfo> 
	inline void DevCreateInstance(T&& info) {
        volkInitialize();

        if (info.m_debug && !DevCheckValidationLayerSupport(info.m_validationLayers)) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = info.m_name.c_str();
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Vienna Vulkan Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(2, 0, 0);
        appInfo.apiVersion = info.m_apiVersion;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        #ifdef __APPLE__
		createInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		#endif

        auto extensions = ToCharPtr(info.m_instanceExtensions);
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

		auto layers = ToCharPtr(info.m_validationLayers);		
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (info.m_debug) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
            createInfo.ppEnabledLayerNames = layers.data();
            DevPopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &info.m_instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
        volkInstance = info.m_instance;

   		volkLoadInstance(info.m_instance);
		
		if (vkEnumerateInstanceVersion) {
			VkResult result = vkEnumerateInstanceVersion(&info.m_apiVersion);
		} else {
			info.m_apiVersion = VK_MAKE_VERSION(1, 1, 0);
		}

		std::cout << "Vulkan API Version available on this system: " << info.m_apiVersion <<  
			" Major: " << VK_VERSION_MAJOR(info.m_apiVersion) << 
			" Minor: " << VK_VERSION_MINOR(info.m_apiVersion) << 
			" Patch: " << VK_VERSION_PATCH(info.m_apiVersion) << std::endl;
    }
	

	//---------------------------------------------------------------------------------------------

	struct DevDestroyDebugUtilsMessengerEXTInfo {
		const VkInstance& 				m_instance;
		const VkDebugUtilsMessengerEXT& m_debugMessenger;
		const VkAllocationCallbacks* 	m_pAllocator;
	};

	template<typename T = DevDestroyDebugUtilsMessengerEXTInfo> 
	inline void DevDestroyDebugUtilsMessengerEXT(T&& info) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(info.m_instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(info.m_instance, info.m_debugMessenger, info.m_pAllocator);
        }
    }

    //---------------------------------------------------------------------------------------------

	struct DevInitVMAInfo {
		VkInstance& 		m_instance;
		VkPhysicalDevice& 	m_physicalDevice;
		VkDevice& 			m_device;
		uint32_t& 			m_apiVersion;
		VmaAllocator& 		m_vmaAllocator;
	};
    
	template<typename T = DevInitVMAInfo>
	inline void DevInitVMA(T&& info) {
        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        allocatorCreateInfo.vulkanApiVersion =  info.m_apiVersion;
        allocatorCreateInfo.physicalDevice = info.m_physicalDevice;
        allocatorCreateInfo.device = info.m_device;
        allocatorCreateInfo.instance = info.m_instance;
        allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
        vmaCreateAllocator(&allocatorCreateInfo, &info.m_vmaAllocator);
    }

	//---------------------------------------------------------------------------------------------

	struct DevCleanupSwapChainInfo {
		const VkDevice& 	m_device;
		const VmaAllocator& m_vmaAllocator;
		const SwapChain& 	m_swapChain;
		const DepthImage& 	m_depthImage;
	};
    
	template<typename T = DevCleanupSwapChainInfo>
    inline void DevCleanupSwapChain(T&& info) {
        vkDestroyImageView(info.m_device, info.m_depthImage.m_depthImageView, nullptr);

        ImgDestroyImage({
			.m_device = info.m_device, 
			.m_vmaAllocator = info.m_vmaAllocator, 
			.m_image = info.m_depthImage.m_depthImage, 
			.m_imageAllocation = info.m_depthImage.m_depthImageAllocation
		});

        for (auto framebuffer : info.m_swapChain.m_swapChainFramebuffers) {
            vkDestroyFramebuffer(info.m_device, framebuffer, nullptr);
        }

        for (auto imageView : info.m_swapChain.m_swapChainImageViews) {
            vkDestroyImageView(info.m_device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(info.m_device, info.m_swapChain.m_swapChain, nullptr);
    }

	//---------------------------------------------------------------------------------------------

	struct DevRecreateSwapChainInfo {
		const SDL_Window* 		m_window;
		const VkSurfaceKHR& 	m_surface;
		const VkPhysicalDevice&	m_physicalDevice;
		const VkDevice& 		m_device;
		const VmaAllocator& 	m_vmaAllocator;
		SwapChain& 			m_swapChain; 
		DepthImage& 		m_depthImage;
		VkRenderPass& 		m_renderPass;
	};
    
	template<typename T = DevRecreateSwapChainInfo>
	inline void DevRecreateSwapChain(T&& info) {
        int width = 0, height = 0;
        
        SDL_GetWindowSize((SDL_Window*)info.m_window, &width, &height);
        while (width == 0 || height == 0) {
            SDL_Event event;
            SDL_WaitEvent(&event);
            SDL_GetWindowSize((SDL_Window*)info.m_window, &width, &height);
        }

        vkDeviceWaitIdle(info.m_device);

        DevCleanupSwapChain(info);
        DevCreateSwapChain(info);
        DevCreateImageViews(info);
        RenCreateDepthResources(info);
        RenCreateFramebuffers(info);
    }

	//---------------------------------------------------------------------------------------------

	struct DevCreateSurfaceInfo {
		const VkInstance& m_instance;
		const SDL_Window*& m_window;
		VkSurfaceKHR& m_surface;
	};
    
	template<typename T = DevCreateSurfaceInfo>
	inline void DevCreateSurface(T&& info) {
        if (SDL_Vulkan_CreateSurface(info.m_window, info.m_instance, nullptr, &info.m_surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }
    }

	//---------------------------------------------------------------------------------------------

	struct DevPickPhysicalDeviceInfo{
		const VkInstance& 				m_instance; 
		const std::vector<std::string>& m_deviceExtensions;
		const VkSurfaceKHR& 			m_surface; 
		uint32_t& 						m_apiVersion; 
		VkPhysicalDevice& 				m_physicalDevice;
	};

	template<typename T = DevPickPhysicalDeviceInfo>
	inline void DevPickPhysicalDevice(T&& info) {

        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(info.m_instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(info.m_instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            VkPhysicalDeviceProperties2 deviceProperties2{};
            deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            vkGetPhysicalDeviceProperties2(device, &deviceProperties2);

            if (deviceProperties2.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 
                	&& DevIsDeviceSuitable({ device, info.m_deviceExtensions, info.m_surface })
					&& VK_VERSION_MINOR(deviceProperties2.properties.apiVersion) >= VK_VERSION_MINOR(info.m_apiVersion) ) {
				info.m_physicalDevice = device;
				info.m_apiVersion = deviceProperties2.properties.apiVersion;
                break;
            }
        }

        if (info.m_physicalDevice == VK_NULL_HANDLE) {
            for (const auto& device : devices) {
				VkPhysicalDeviceProperties2 deviceProperties2{};
				deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				vkGetPhysicalDeviceProperties2(device, &deviceProperties2);

				if (DevIsDeviceSuitable( { device, info.m_deviceExtensions, info.m_surface })
						&& VK_VERSION_MINOR(deviceProperties2.properties.apiVersion) >= VK_VERSION_MINOR(info.m_apiVersion)) {
					info.m_physicalDevice = device;
					info.m_apiVersion = deviceProperties2.properties.apiVersion;
                    break;
                }
            }
        }

        if (info.m_physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

	//---------------------------------------------------------------------------------------------

    struct DevCreateLogicalDeviceInfo {
		const VkSurfaceKHR& 			m_surface;
		const VkPhysicalDevice&			m_physicalDevice;
		const std::vector<std::string>& m_validationLayers;
		const std::vector<std::string>& m_deviceExtensions; 
		const bool& m_debug; 
		QueueFamilyIndices& m_queueFamilies;
		VkDevice& 	m_device; 
		VkQueue& 	m_graphicsQueue;
		VkQueue& 	m_presentQueue;
	};

	template<typename T = DevCreateLogicalDeviceInfo>
	inline void DevCreateLogicalDevice(T&& info) {

		info.m_queueFamilies = DevFindQueueFamilies(info);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { info.m_queueFamilies.graphicsFamily.value(), info.m_queueFamilies.presentFamily.value()};

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		createInfo.pEnabledFeatures = &deviceFeatures;

		auto extensions = ToCharPtr(info.m_deviceExtensions);
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		auto layers = ToCharPtr(info.m_validationLayers);
		if (info.m_debug) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
			createInfo.ppEnabledLayerNames = layers.data();
		} else {
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(info.m_physicalDevice, &createInfo, nullptr, &info.m_device) != VK_SUCCESS) {
			throw std::runtime_error("failed to create logical device!");
		}

		volkLoadDevice(info.m_device);

		vkGetDeviceQueue(info.m_device, info.m_queueFamilies.graphicsFamily.value(), 0, &info.m_graphicsQueue);
		vkGetDeviceQueue(info.m_device, info.m_queueFamilies.presentFamily.value(), 0, &info.m_presentQueue);
	}

	//---------------------------------------------------------------------------------------------

	inline auto DevChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) -> VkSurfaceFormatKHR {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

	//---------------------------------------------------------------------------------------------

    inline auto DevChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) -> VkPresentModeKHR {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;    
    }

	//---------------------------------------------------------------------------------------------

    inline auto DevChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const SDL_Window* window) -> VkExtent2D {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            SDL_GetWindowSize( (SDL_Window*)window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width
                , capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height
                , capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

	//---------------------------------------------------------------------------------------------

    struct DevCreateSwapChainInfo {
		const SDL_Window*		m_window;
		const VkSurfaceKHR& 	m_surface;
		const VkPhysicalDevice& m_physicalDevice;
		const VkDevice& 		m_device; 
		SwapChain& 				m_swapChain;
	};

	template<typename T = DevCreateSwapChainInfo>
	inline void DevCreateSwapChain(T&& info) {
        SwapChainSupportDetails swapChainSupport = DevQuerySwapChainSupport(info);

        VkSurfaceFormatKHR surfaceFormat = DevChooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = DevChooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = DevChooseSwapExtent(swapChainSupport.capabilities, info.m_window);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = info.m_surface;

        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = DevFindQueueFamilies(info);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(info.m_device, &createInfo, nullptr, &info.m_swapChain.m_swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(info.m_device, info.m_swapChain.m_swapChain, &imageCount, nullptr);
        info.m_swapChain.m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(info.m_device, info.m_swapChain.m_swapChain, &imageCount, info.m_swapChain.m_swapChainImages.data());

        info.m_swapChain.m_swapChainImageFormat = surfaceFormat.format;
        info.m_swapChain.m_swapChainExtent = extent;
    }

	//---------------------------------------------------------------------------------------------
    struct DevCreateImageViewsInfo{
		const VkDevice& m_device;
		SwapChain& 		m_swapChain;
	};

	template<typename T = DevCreateImageViewsInfo>
    inline void DevCreateImageViews(T&& info) {
        info.m_swapChain.m_swapChainImageViews.resize(info.m_swapChain.m_swapChainImages.size());

        for (uint32_t i = 0; i < info.m_swapChain.m_swapChainImages.size(); i++) {
            info.m_swapChain.m_swapChainImageViews[i] = ImgCreateImageView2({
					.m_device = info.m_device, 
					.m_image = info.m_swapChain.m_swapChainImages[i], 
					.m_format = info.m_swapChain.m_swapChainImageFormat, 
					.m_aspects = VK_IMAGE_ASPECT_COLOR_BIT
				});
        }
    }



} // namespace vh
