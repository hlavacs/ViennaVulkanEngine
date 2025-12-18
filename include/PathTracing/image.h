#pragma once

namespace vve {

    struct LayoutInfo {
        VkAccessFlags accessMask;
        VkPipelineStageFlags stageMask;
    };

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
        Image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice);

        Image(uint8_t* pixels, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice);

        Image(VkImage image, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice);

        void setLayout(VkImageLayout layout) {
            currentLayout = layout;
        }

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


        void transitionImageLayout(VkImageLayout newLayout);

        void recordImageLayoutTransition(VkImageLayout newLayout, int currentFrame);

        void copyDataToImage(uint8_t* pixels);
        void copyFromImage(Image* srcImage, VkImageLayout newLayout);
        void recordCopyFromImage(Image* srcImage, VkImageLayout newLayout, int currentFrame);

        VkImage getImage();
        VkImageView getImageView();

        VkFormat getFormat();

        ~Image();
    };

}