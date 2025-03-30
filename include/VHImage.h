#pragma once


namespace vh {
    
	void ImgCreateTextureImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, VkQueue graphicsQueue, VkCommandPool commandPool, void* pixels, int width, int height, size_t size, Map& texture);

    void ImgCreateTextureImageView(VkDevice device, Map& texture);

    void ImgCreateTextureSampler(VkPhysicalDevice physicalDevice, VkDevice device, Map &texture);

    VkImageView ImgCreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    VkImageView ImgCreateImageView2(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t layers, uint32_t mipLevels);

    void ImgCreateImage(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator, uint32_t width, uint32_t height
        , VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageLayout imageLayout
        , VkMemoryPropertyFlags properties, VkImage& image, VmaAllocation& imageAllocation);
        
    void ImgCreateImage2(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , uint32_t width, uint32_t height, uint32_t depth, uint32_t layers, uint32_t mipLevels
        , VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageLayout imageLayout
        , VkMemoryPropertyFlags properties, VkImage& image, VmaAllocation& imageAllocation);

    void ImgDestroyImage(VkDevice device, VmaAllocator vmaAllocator, VkImage image, VmaAllocation& imageAllocation);

    void ImgTransitionImageLayout(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    void ImgTransitionImageLayout2(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkImage image, VkFormat format
        , VkImageAspectFlags aspect, int numMipLevels, int numLayers
        , VkImageLayout oldLayout, VkImageLayout newLayout);

    VkResult ImgCopyImageToHost(VkDevice device, VmaAllocator allocator, VkQueue graphicsQueue, 
        VkCommandPool commandPool, VkImage image, VkFormat format, VkImageAspectFlagBits aspect, VkImageLayout layout,
        /*gli::byte*/ unsigned char *bufferData, uint32_t width, uint32_t height, uint32_t imageSize, int r, int g, int b, int a);
    
	void ImgSwapChannels(unsigned char *bufferData, int r, int g, int b, int a, int width, int height);

    void ImgPickDepthMapFormat( VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& depthFormats, VkFormat& depthMapFormat );
}

