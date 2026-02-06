#pragma once

/**
 * @file render_target.h
 * @brief Render target images and clear state.
 */

namespace vve {
	/** Wrapper for per-frame render target images. */
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

		/**
		 * Create a render target with a default clear color.
		 * @param width Target width in pixels.
		 * @param height Target height in pixels.
		 * @param format Image format.
		 * @param usage Image usage flags.
		 * @param aspectFlags Image aspect flags.
		 * @param commandManager Command manager for image transitions.
		 * @param device Logical device.
		 * @param physicalDevice Physical device for memory queries.
		 */
		RenderTarget(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice);

		/**
		 * Create a render target with a custom clear color.
		 * @param width Target width in pixels.
		 * @param height Target height in pixels.
		 * @param format Image format.
		 * @param usage Image usage flags.
		 * @param aspectFlags Image aspect flags.
		 * @param commandManager Command manager for image transitions.
		 * @param device Logical device.
		 * @param physicalDevice Physical device for memory queries.
		 * @param clearColorValue Clear color value.
		 */
		RenderTarget(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice, VkClearColorValue clearColorValue);

		/** Release owned image resources. */
		~RenderTarget();

		/**
		 * @param currentFrame Frame index.
		 * @return Image for the given frame index.
		 */
		Image* getImage(int currentFrame);

		/** @return Render target image format. */
		VkFormat getFormat();

		/**
		 * Recreate all images with a new size.
		 * @param width New width in pixels.
		 * @param height New height in pixels.
		 */
		void recreateRenderTarget(uint32_t width, uint32_t height);
		
		/** @return Clear color value for the target. */
		VkClearValue getClearColor();
	};

}
