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

		RenderTarget(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice);

		RenderTarget(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice, VkClearColorValue clearColorValue);

		~RenderTarget();

		Image* getImage(int currentFrame);

		VkFormat getFormat();

		void recreateRenderTarget(uint32_t width, uint32_t height);
		
		VkClearValue getClearColor();
	};

}