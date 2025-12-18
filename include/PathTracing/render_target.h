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

	public:

		RenderTarget(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
			commandManager(commandManager), device(device), physicalDevice(physicalDevice), format(format) {
			for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				Image* image = new Image(width, height, format, VK_IMAGE_TILING_OPTIMAL, usage, aspectFlags, commandManager, device, physicalDevice);
				images.push_back(image);
			}
		}

		//recreate RenderTarget needed, if swapchain is recreated!!!!!


		Image* getImage(int currentFrame) {
			return images[currentFrame];
		}

		VkFormat getFormat() {
			return format;
		}



	};

}