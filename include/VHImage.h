#pragma once


namespace vh {

	void ImgCreateTextureImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, VkQueue graphicsQueue, VkCommandPool commandPool, void* pixels, int width, int height, size_t size, Map& texture);

    void ImgCreateTextureImageView(VkDevice device, Map& texture);

    void ImgCreateTextureSampler(VkPhysicalDevice physicalDevice, VkDevice device, Map &texture);

    VkImageView ImgCreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    void ImgCreateImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, uint32_t width, uint32_t height
        , VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties
        , VkImage& image, VmaAllocation& imageAllocation);

    void ImgDestroyImage(VkDevice device, VmaAllocator vmaAllocator, VkImage image, VmaAllocation& imageAllocation);

    void ImgTransitionImageLayout(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    VkResult ImgCopySwapChainImageToHost(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, 
        VkCommandPool commandPool, VkImage image, VkFormat format, VkImageAspectFlagBits aspect, VkImageLayout layout,
        /*gli::byte*/ unsigned char *bufferData, uint32_t width, uint32_t height, uint32_t imageSize);
    
    VkResult ImgCopyImageToHost(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, 
        VkCommandPool commandPool, VkImage image, VkFormat format, VkImageAspectFlagBits aspect, VkImageLayout layout,
        /*gli::byte*/ unsigned char *bufferData, uint32_t width, uint32_t height, uint32_t imageSize);
    
}

