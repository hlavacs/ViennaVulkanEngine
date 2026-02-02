#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
	RenderTarget::RenderTarget(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
		commandManager(commandManager), device(device), physicalDevice(physicalDevice), format(format) {
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			Image* image = new Image(width, height, format, VK_IMAGE_TILING_OPTIMAL, usage, aspectFlags, commandManager, device, physicalDevice);
			images.push_back(image);
		}
		clearColor.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	}

	RenderTarget::RenderTarget(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice, VkClearColorValue clearColorValue) :
		commandManager(commandManager), device(device), physicalDevice(physicalDevice), format(format) {
		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			Image* image = new Image(width, height, format, VK_IMAGE_TILING_OPTIMAL, usage, aspectFlags, commandManager, device, physicalDevice);
			images.push_back(image);
		}
		clearColor.color = clearColorValue;
	}

	RenderTarget::~RenderTarget() {
		for (Image* image : images) {
			delete image;
		}
	}


	Image* RenderTarget::getImage(int currentFrame) {
		return images[currentFrame];
	}

	VkFormat RenderTarget::getFormat() {
		return format;
	}

	void RenderTarget::recreateRenderTarget(uint32_t width, uint32_t height) {
		for (Image* image : images) {
			image->recreateImage(width, height);
		}
	}

	VkClearValue RenderTarget::getClearColor() {
		return clearColor;
	}

}