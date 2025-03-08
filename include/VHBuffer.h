#pragma once


namespace vh {

    void BufCreateBuffer( VmaAllocator vmaAllocator
        , VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties
        , VmaAllocationCreateFlags vmaFlags, VkBuffer& buffer
        , VmaAllocation& allocation, VmaAllocationInfo* allocationInfo = nullptr);

    void BufDestroyBuffer(VkDevice device, VmaAllocator vmaAllocator, VkBuffer buffer, VmaAllocation& allocation);

    void BufDestroyBuffer2(VkDevice device, VmaAllocator vmaAllocator, UniformBuffers buffers);

    void BufCopyBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void BufCopyBufferToImage(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool
        , VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);



    VkResult BufCopyImageToBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkImage image, VkImageAspectFlagBits aspect, VkBuffer buffer, uint32_t layerCount, uint32_t width, uint32_t height);
    VkResult BufCopyImageToBuffer(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkImage image, VkBuffer buffer, std::vector<VkBufferImageCopy> &regions, uint32_t width, uint32_t height);



    void BufCreateVertexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Mesh& geometry);

    void BufCreateIndexBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , VkQueue graphicsQueue, VkCommandPool commandPool, Mesh& geometry);

    void BufCreateUniformBuffers(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator& vmaAllocator, 
		VkDeviceSize bufferSize, UniformBuffers &uniformBuffers);
        

}; // namespace vh