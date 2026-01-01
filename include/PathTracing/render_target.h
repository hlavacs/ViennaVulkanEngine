#pragma once

namespace vve {
	class RenderTarget {
	private:
		std::vector<Image*> images;
		bool persistant = false;
		CommandManager* commandManager;
		VkDevice device;
		VkPhysicalDevice physicalDevice;
		VkFormat format;
		VkClearValue clearColor;

	public:

		RenderTarget(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
			commandManager(commandManager), device(device), physicalDevice(physicalDevice), format(format) {
			for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				Image* image = new Image(width, height, format, VK_IMAGE_TILING_OPTIMAL, usage, aspectFlags, commandManager, device, physicalDevice);
				images.push_back(image);
			}
			clearColor.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		}

		RenderTarget(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice, VkClearColorValue clearColorValue) :
			commandManager(commandManager), device(device), physicalDevice(physicalDevice), format(format) {
			for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				Image* image = new Image(width, height, format, VK_IMAGE_TILING_OPTIMAL, usage, aspectFlags, commandManager, device, physicalDevice);
				images.push_back(image);
			}
			clearColor.color = clearColorValue;
		}


		Image* getImage(int currentFrame) {
			return images[currentFrame];
		}

		VkFormat getFormat() {
			return format;
		}

		void recreateRenderTarget(uint32_t width, uint32_t height) {
			for (Image* image : images) {
				image->recreateImage(width, height);
			}
		}

		VkClearValue getClearColor() {
			return clearColor;
		}

	};

}