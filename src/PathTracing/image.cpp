#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

    Image::Image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
        GPUDataStorage(device, physicalDevice), width(width), height(height), format(format), tiling(tiling), usage(usage), aspectFlags(aspectFlags), commandManager(commandManager)
    {
        createImage();
        createImageView();
        currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    //for the swapchain
    Image::Image(VkImage image, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
        GPUDataStorage(device, physicalDevice), image(image), width(width), height(height), format(format), tiling(tiling), usage(usage), aspectFlags(aspectFlags), commandManager(commandManager) {
        createImageView();
        currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    Image::Image(uint8_t* pixels, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlags aspectFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
        GPUDataStorage(device, physicalDevice), width(width), height(height), format(format), tiling(tiling), usage(usage), aspectFlags(aspectFlags), commandManager(commandManager)
    {
        createImage();
        currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copyDataToImage(pixels);
        createImageView();
    }

    void Image::recreateImage(uint32_t width, uint32_t height) {
        this->width = width;
        this->height = height;

        if (imageView) vkDestroyImageView(device, imageView, nullptr);
        if (image) vkDestroyImage(device, image, nullptr);
        if (imageMemory) vkFreeMemory(device, imageMemory, nullptr);

        createImage();
        createImageView();
        currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    Image::~Image() {
        if (imageView) vkDestroyImageView(device, imageView, nullptr);
        if (image) vkDestroyImage(device, image, nullptr);
        if (imageMemory) vkFreeMemory(device, imageMemory, nullptr);
    }

    void Image::createImageView() {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }
    }

    void Image::transitionImageLayout(VkImageLayout newLayout, VkCommandBuffer cmd) {

        if (newLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
            std::cout << "New image layout may not be VK_IMAGE_LAYOUT_UNDEFINED \n";
        }
        

        if (newLayout == currentLayout) {
            return;
        }

        LayoutInfo oldInfo = getLayoutInfo(currentLayout);
        LayoutInfo newInfo = getLayoutInfo(newLayout);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = currentLayout;
        barrier.newLayout = newLayout;
        barrier.srcAccessMask = oldInfo.accessMask;
        barrier.dstAccessMask = newInfo.accessMask;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            cmd,
            oldInfo.stageMask,
            newInfo.stageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
        currentLayout = newLayout;
    }

    void Image::transitionImageLayout(VkImageLayout newLayout) {
        VkCommandBuffer cmd = commandManager->beginSingleTimeCommand();

        transitionImageLayout(newLayout, cmd);

        commandManager->endSingleTimeCommand(cmd);
    }


    void Image::recordImageLayoutTransition(VkImageLayout newLayout, int currentFrame) {
        VkCommandBuffer cmd = commandManager->getCommandBuffer(currentFrame);
        transitionImageLayout(newLayout, cmd);
    }

    void Image::copyBufferToImage(VkBuffer& buffer) {
        VkCommandBuffer commandBuffer = commandManager->beginSingleTimeCommand();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );


        commandManager->endSingleTimeCommand(commandBuffer);
    }

    void Image::copyDataToImage(uint8_t* pixels) {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        VkDeviceSize imageSize = width * height * BytesPerPixel();

        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer);
        transitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void Image::copyFromImage(Image* srcImage, VkImageLayout newLayout, VkCommandBuffer cmd) {
        if (width != srcImage->width || height != srcImage->height) {
            throw std::runtime_error("Source and destination images must have the same dimensions and format");
        }

        /*
        if (width != srcImage->width || height != srcImage->height || format != srcImage->format) {
            throw std::runtime_error("Source and destination images must have the same dimensions and format");
        }
        */

        VkImageLayout otherImageLayout = srcImage->currentLayout;

        srcImage->transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmd);
        transitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd);

        VkImageSubresourceLayers subresource{};
        subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresource.baseArrayLayer = 0;
        subresource.mipLevel = 0;
        subresource.layerCount = 1;

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = subresource;
        copyRegion.dstSubresource = subresource;
        copyRegion.srcOffset = { 0, 0, 0 };
        copyRegion.dstOffset = { 0, 0, 0 };
        copyRegion.extent.width = width;
        copyRegion.extent.height = height;
        copyRegion.extent.depth = 1;


        vkCmdCopyImage(cmd, srcImage->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        transitionImageLayout(newLayout, cmd);
        srcImage->transitionImageLayout(otherImageLayout, cmd);
    }

    void Image::copyFromImage(Image* srcImage, VkImageLayout newLayout) {
        VkCommandBuffer cmd = commandManager->beginSingleTimeCommand();
        copyFromImage(srcImage, newLayout, cmd);
        commandManager->endSingleTimeCommand(cmd);
    }

    void Image::recordCopyFromImage(Image* srcImage, VkImageLayout newLayout, int currentFrame) {
        VkCommandBuffer cmd = commandManager->getCommandBuffer(currentFrame);
        copyFromImage(srcImage, newLayout, cmd);
    }

    void Image::createImage() {

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    VkImage Image::getImage() {
        return image;
    }
    VkImageView Image::getImageView() {
        return imageView;
    }

    VkFormat Image::getFormat() {
        return format;
    }

    uint32_t Image::BytesPerPixel() {
        switch (format) {
            // ---- 8-bit per channel formats ----
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_SINT:
            return 1;

        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SRGB:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8_SINT:
            return 2;

        case VK_FORMAT_R8G8B8_UNORM:
        case VK_FORMAT_R8G8B8_SRGB:
        case VK_FORMAT_R8G8B8_UINT:
        case VK_FORMAT_R8G8B8_SINT:
            return 3;

        case VK_FORMAT_B8G8R8_UNORM:
        case VK_FORMAT_B8G8R8_SRGB:
        case VK_FORMAT_B8G8R8_UINT:
        case VK_FORMAT_B8G8R8_SINT:
            return 3;

        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_SINT:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UINT:
        case VK_FORMAT_B8G8R8A8_SINT:
            return 4;

            // ---- 16-bit formats ----
        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16_SFLOAT:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT:
            return 2;

        case VK_FORMAT_R16G16_UNORM:
        case VK_FORMAT_R16G16_SFLOAT:
        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16_SINT:
            return 4;

        case VK_FORMAT_R16G16B16_UNORM:
        case VK_FORMAT_R16G16B16_SFLOAT:
        case VK_FORMAT_R16G16B16_UINT:
        case VK_FORMAT_R16G16B16_SINT:
            return 6;

        case VK_FORMAT_R16G16B16A16_UNORM:
        case VK_FORMAT_R16G16B16A16_SFLOAT:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R16G16B16A16_SINT:
            return 8;

            // ---- 32-bit float formats ----
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
            return 4;

        case VK_FORMAT_R32G32_SFLOAT:
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT:
            return 8;

        case VK_FORMAT_R32G32B32_SFLOAT:
        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT:
            return 12;

        case VK_FORMAT_R32G32B32A32_SFLOAT:
        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
            return 16;

        default:
            throw std::runtime_error("Unsupported format in BytesPerPixel()");
        }
    }

}