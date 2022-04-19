/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VHHelper.h"

namespace vh
{
	//-------------------------------------------------------------------------------------------------------

	/**
		*
		* \brief Choose swapchain format - try to use VK_FORMAT_B8G8R8A8_UNORM
		*
		* \param[in] availableFormats A list of available swap chain formats
		* \returns the chosen swapchain format
		*/
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
	{
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		for (const auto &availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	/**
		*
		* \brief Choose the swapchain present mode - try to use Mailbox
		*
		* \param[in] availablePresentModes A list of available swap chain present modes
		* \returns the chosen swapchain format
		*/
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
	{
		VkPresentModeKHR bestMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

		for (const auto &availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
			else if (availablePresentMode == VK_PRESENT_MODE_FIFO_KHR)
			{
				bestMode = availablePresentMode;
			}
		}

		return bestMode;
	}

	/**
		*
		* \brief Choose the 2D extent of the swapchain
		*
		* \param[in] capabilities Capabilities of the window surface
		* \param[in] extent Extent of the window
		* \returns the chosen swapchain extent
		*/
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, VkExtent2D extent)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent = extent;

			actualExtent.width = std::max(capabilities.minImageExtent.width,
				std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height,
				std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	//-------------------------------------------------------------------------------------------------------
	/**
		*
		* \brief Choose the swapchain
		*
		* \param[in] physicalDevice The physical device
		* \param[in] surface The window surface
		* \param[in] device Logical device
		* \param[in] frameBufferExtent Extent of the framebuffer
		* \param[out] swapChain The new swapchain
		* \param[out] swapChainImages A list containing the swapchain images
		* \param[out] swapChainImageViews A list containing swapchain image views
		* \param[out] swapChainImageFormat A list containing swapchain image formats
		* \param[out] swapChainExtent Swapchain extent
		*
		*/
	VkResult vhSwapCreateSwapChain(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device, VkExtent2D frameBufferExtent, VkSwapchainKHR *swapChain, std::vector<VkImage> &swapChainImages, std::vector<VkImageView> &swapChainImageViews, VkFormat *swapChainImageFormat, VkExtent2D *swapChainExtent)
	{
		SwapChainSupportDetails swapChainSupport = vhDevQuerySwapChainSupport(physicalDevice, surface);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D swapextent = chooseSwapExtent(swapChainSupport.capabilities, frameBufferExtent);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 &&
			imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;

		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = swapextent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
			VK_IMAGE_USAGE_STORAGE_BIT; //need for copying to host

		QueueFamilyIndices indices = vhDevFindQueueFamilies(physicalDevice, surface);
		uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };

		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, swapChain) != VK_SUCCESS)
		{
			assert(false);
			exit(1);
		}

		VHCHECKRESULT(vkGetSwapchainImagesKHR(device, *swapChain, &imageCount, nullptr));
		swapChainImages.resize(imageCount);
		VHCHECKRESULT(vkGetSwapchainImagesKHR(device, *swapChain, &imageCount, swapChainImages.data()));

		*swapChainImageFormat = surfaceFormat.format;
		*swapChainExtent = swapextent;

		swapChainImageViews.resize(swapChainImages.size());
		for (uint32_t i = 0; i < swapChainImages.size(); i++)
		{
			VHCHECKRESULT(vhBufCreateImageView(device, swapChainImages[i], *swapChainImageFormat,
				VK_IMAGE_VIEW_TYPE_2D, 1, VK_IMAGE_ASPECT_COLOR_BIT,
				&swapChainImageViews[i]));
		}
		return VK_SUCCESS;
	}
} // namespace vh
