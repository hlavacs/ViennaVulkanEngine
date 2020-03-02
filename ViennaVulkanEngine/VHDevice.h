#pragma once

#include "VHInclude.h"

namespace vh::dev {


	VkResult vhCreateInstance(VhVulkanState &state, std::vector<const char*>& extensions, std::vector<const char*>& validationLayers );

	VkResult vhPickPhysicalDevice(VhVulkanState& state, std::vector<const char*> requiredExtensions);

	VhQueueFamilyIndices vhFindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

	VkFormat vhFindSupportedFormat(VhVulkanState& state, const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features);

	VkFormat vhFindDepthFormat(VhVulkanState& state);

	VhSwapChainSupportDetails vhQuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);


	//--------------------------------------------------------------------------------------------------------------------------------
	//logical device
	VkResult vhCreateLogicalDevice(VhVulkanState& state, std::vector<const char*> requiredDeviceExtensions,
		std::vector<const char*> requiredValidationLayers);



}

