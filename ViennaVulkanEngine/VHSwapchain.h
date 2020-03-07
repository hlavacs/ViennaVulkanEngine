#pragma once

/**
*
* \file
* \brief
*
* Details
*
*/

namespace vh::swap {


	//--------------------------------------------------------------------------------------------------------------------------------
	//swapchain
	
	VkResult vhCreateSwapChain(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device,
		VkExtent2D frameBufferExtent, VkSwapchainKHR* swapChain,
		std::vector<VkImage>& swapChainImages, std::vector<VkImageView>& swapChainImageViews,
		VkFormat* swapChainImageFormat, VkExtent2D* swapChainExtent);


}


