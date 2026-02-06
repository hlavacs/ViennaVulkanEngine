/**
 * @file buffer.cpp
 * @brief Implementation of Vulkan buffer helpers.
 */

#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
    /**
     * @param typeFilter Bitmask of acceptable memory types.
     * @param properties Required memory property flags.
     * @return Memory type index compatible with the request.
     */
    uint32_t GPUDataStorage::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");

    }

    /**
     * @param size Buffer size in bytes.
     * @param usage Buffer usage flags.
     * @param properties Required memory property flags.
     * @param allocFlags Allocation flags (e.g., device address).
     * @param buffer Output buffer handle.
     * @param bufferMemory Output memory allocation.
     */
    void GPUDataStorage::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryAllocateFlags allocFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateFlagsInfo allocFlagInfo{};
        allocFlagInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        allocFlagInfo.flags = allocFlags;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        allocInfo.pNext = (allocFlags != 0) ? &allocFlagInfo : nullptr;


        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        /// Bind buffer to its allocated memory at offset 0.
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }
}
