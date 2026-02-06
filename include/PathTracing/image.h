#pragma once

/**
 * @file image.h
 * @brief Image wrapper with layout transitions and data transfer helpers.
 */

namespace vve {

    /** Access and stage masks used for an image layout transition. */
    struct LayoutInfo {
        VkAccessFlags accessMask;
        VkPipelineStageFlags stageMask;
    };

    /** Vulkan image wrapper with optional data upload and layout tracking. */
    class Image : public GPUDataStorage {
    private:

        VkImage image;
        VkImageView imageView;
        VkDeviceMemory imageMemory;
        VkFormat format;
        VkImageTiling tiling;
        VkImageUsageFlags usage;
        uint32_t width;
        uint32_t height;
        CommandManager* commandManager;
        VkImageLayout currentLayout;

        VkImageAspectFlags aspectFlags;

        void createImage();
        void createImageView();
        void copyBufferToImage(VkBuffer& buffer);
        uint32_t BytesPerPixel();



        LayoutInfo getLayoutInfo(VkImageLayout layout) {
            switch (layout) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                return { 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                return { VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                return { VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                return { VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                return { VK_ACCESS_MEMORY_READ_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                return { VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            case VK_IMAGE_LAYOUT_GENERAL:
                return {
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR };

            default:
                throw std::runtime_error("Unsupported image layout!");
            }
        }


        void transitionImageLayout(VkImageLayout newLayout, VkCommandBuffer cmd);
        void copyFromImage(Image* srcImage, VkImageLayout newLayout, VkCommandBuffer cmd);


    public:
        /**
         * Create an owned image with allocated memory.
         * @param width Image width in pixels.
         * @param height Image height in pixels.
         * @param format Image format.
         * @param tiling Image tiling mode.
         * @param usage Image usage flags.
         * @param aspectFlags Image aspect flags.
         * @param commandManager Command manager for layout transitions.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        Image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice);

        /**
         * Create an owned image and upload initial pixel data.
         * @param pixels Source pixel buffer.
         * @param width Image width in pixels.
         * @param height Image height in pixels.
         * @param format Image format.
         * @param tiling Image tiling mode.
         * @param usage Image usage flags.
         * @param aspectFlags Image aspect flags.
         * @param commandManager Command manager for layout transitions.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        Image(uint8_t* pixels, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice);

        /**
         * Wrap an existing image without taking ownership of creation.
         * @param image Existing Vulkan image handle.
         * @param width Image width in pixels.
         * @param height Image height in pixels.
         * @param format Image format.
         * @param tiling Image tiling mode.
         * @param usage Image usage flags.
         * @param aspectFlags Image aspect flags.
         * @param commandManager Command manager for layout transitions.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        Image(VkImage image, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice);

        /**
         * Set the internally tracked layout without a barrier.
         * @param layout Layout to track.
         */
        void setLayout(VkImageLayout layout) {
            currentLayout = layout;
        }

        /**
         * Insert an image memory barrier for the current layout.
         * @param srcAccessMask Access mask of previous usage.
         * @param srcStage Pipeline stage of previous usage.
         * @param dstAccessMask Access mask of next usage.
         * @param dstStage Pipeline stage of next usage.
         * @param currentFrame Frame index for command buffer selection.
         */
        void memoryBarrier(VkAccessFlags srcAccessMask, VkPipelineStageFlags srcStage, VkAccessFlags dstAccessMask, VkPipelineStageFlags dstStage, int currentFrame) {
            VkImageMemoryBarrier imgBarrier{};
            imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imgBarrier.srcAccessMask = srcAccessMask;
            imgBarrier.dstAccessMask = dstAccessMask;
            imgBarrier.oldLayout = currentLayout; // should match current layout
            imgBarrier.newLayout = currentLayout;
            imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.image = image;
            imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imgBarrier.subresourceRange.baseMipLevel = 0;
            imgBarrier.subresourceRange.levelCount = 1;
            imgBarrier.subresourceRange.baseArrayLayer = 0;
            imgBarrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                commandManager->getCommandBuffer(currentFrame),
                srcStage,         // stage that wrote the image
                dstStage,          // stage that will read it
                0,
                0, nullptr,            // memory barriers
                0, nullptr,            // buffer barriers
                1, &imgBarrier         // image barriers
            );
        }


        /**
         * Transition the image to a new layout (immediate submit).
         * @param newLayout Target layout.
         */
        void transitionImageLayout(VkImageLayout newLayout);

        /**
         * Record a layout transition into the current frame's command buffer.
         * @param newLayout Target layout.
         * @param currentFrame Frame index.
         */
        void recordImageLayoutTransition(VkImageLayout newLayout, int currentFrame);

        /**
         * Upload pixel data into the image using a staging buffer.
         * @param pixels Source pixel buffer.
         */
        void copyDataToImage(uint8_t* pixels);
        /**
         * Copy from another image with an optional layout transition.
         * @param srcImage Source image.
         * @param newLayout Layout for this image after copy.
         */
        void copyFromImage(Image* srcImage, VkImageLayout newLayout);
        /**
         * Record copy from another image into the frame command buffer.
         * @param srcImage Source image.
         * @param newLayout Layout for this image after copy.
         * @param currentFrame Frame index.
         */
        void recordCopyFromImage(Image* srcImage, VkImageLayout newLayout, int currentFrame);

        /**
         * Recreate the image with a new size.
         * @param width New width in pixels.
         * @param height New height in pixels.
         */
        void recreateImage(uint32_t width, uint32_t height);

        /** @return Vulkan image handle. */
        VkImage getImage();
        /** @return Vulkan image view handle. */
        VkImageView getImageView();

        /** @return Image format. */
        VkFormat getFormat();

        /** Release owned image resources. */
        ~Image();

        /** Remove ownership of the underlying image handle. */
        void removeImageRefernece();
    };

}
