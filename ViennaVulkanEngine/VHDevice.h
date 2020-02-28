#pragma once

#include "VHInclude.h"

namespace vh::dev {


	VkResult vhDevCreateInstance(std::vector<const char*>& extensions, std::vector<const char*>& validationLayers, VkInstance* instance);
	VkResult vhDevPickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface, std::vector<const char*> requiredExtensions,
	VkPhysicalDevice* physicalDevice, VkPhysicalDeviceFeatures* pFeatures, VkPhysicalDeviceLimits* limits);
	QueueFamilyIndices vhDevFindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
	VkFormat vhDevFindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat vhDevFindDepthFormat(VkPhysicalDevice physicalDevice);


}

