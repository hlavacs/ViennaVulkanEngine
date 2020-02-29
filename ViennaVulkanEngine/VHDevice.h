#pragma once

#include "VHInclude.h"

namespace vh::dev {


	VkResult vhCreateInstance(std::vector<const char*>& extensions, std::vector<const char*>& validationLayers, VkInstance* instance);
	VkResult vhPickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, std::vector<const char*> requiredExtensions,
		VkPhysicalDevice* physicalDevice, VkPhysicalDeviceFeatures* pFeatures, VkPhysicalDeviceLimits* limits);
	QueueFamilyIndices vhFindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
	VkFormat vhFindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, 
		VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat vhFindDepthFormat(VkPhysicalDevice physicalDevice);
	SwapChainSupportDetails vhQuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);


	//--------------------------------------------------------------------------------------------------------------------------------
	//logical device
	VkResult vhCreateLogicalDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
		std::vector<const char*> requiredDeviceExtensions,
		std::vector<const char*> requiredValidationLayers,
		VkDevice* device, VkQueue* graphicsQueue, VkQueue* presentQueue);



}

