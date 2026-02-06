#pragma once

/**
 * @file buffer.h
 * @brief Buffer helpers for host-visible and device-local Vulkan buffers.
 */

namespace vve {
    /** Shared Vulkan buffer utilities (memory selection + buffer creation). */
    class GPUDataStorage {

    protected:
        VkPhysicalDevice physicalDevice;
        VkDevice& device;

        GPUDataStorage(VkDevice& device, VkPhysicalDevice& physicalDevice) : device(device), physicalDevice(physicalDevice) {}

        /**
         * Pick a memory type that satisfies typeFilter and the requested flags.
         * @param typeFilter Bitmask of acceptable memory types.
         * @param properties Required memory property flags.
         * @return Memory type index compatible with the request.
         */
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        /**
         * Create a VkBuffer and allocate/bind its backing memory.
         * @param size Buffer size in bytes.
         * @param usage Buffer usage flags.
         * @param properties Required memory property flags.
         * @param allocFlags Allocation flags (e.g., device address).
         * @param buffer Output buffer handle.
         * @param bufferMemory Output memory allocation.
         */
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryAllocateFlags allocFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    };

    /** Raw Vulkan buffer wrapper with optional host mapping. */
    class GenericBuffer : public GPUDataStorage {
    private:
        VkBuffer buffer{};
        VkDeviceMemory memory{};
        void* mappedMemory{};
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags properties;
        VkMemoryAllocateFlags allocFlags;
        VkDeviceSize size;
    public:
        /**
         * @param size Buffer size in bytes (0 is promoted to 1).
         * @param usage Buffer usage flags.
         * @param properties Required memory property flags.
         * @param allocFlags Allocation flags.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        GenericBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : GPUDataStorage(device, physicalDevice), usage(usage), size(size), properties(properties), allocFlags(allocFlags) {
            if (size == 0) {
                size = 1;
            }
            initBuffer();
        }

        /** Destroy and release buffer resources. */
        ~GenericBuffer() {
            destroyBuffer();
        }

        /** Destroy buffer and memory, unmapping if needed. */
        void destroyBuffer() {
            if (mappedMemory) { vkUnmapMemory(device, memory); mappedMemory = nullptr; }
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
        }

        /** Create buffer and map it if host-visible. */
        void initBuffer() {
            createBuffer(size, usage, properties, allocFlags, buffer, memory);
            if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            {
                vkMapMemory(device, memory, 0, size, 0, &mappedMemory);
            }
            else
            {
                mappedMemory = nullptr;
            }
        }

        /** @return Device address of the buffer. */
        VkDeviceAddress getDeviceAddress() {
            VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
            info.buffer = buffer;
            return vkGetBufferDeviceAddress(device, &info);
        }

        /** @return Vulkan buffer handle. */
        VkBuffer getBuffer() const { return buffer; }
    };

    /** Host-visible, typed buffer for CPU updates. */
    template <typename T>
    class HostBuffer : public GPUDataStorage {
    private:
        VkBuffer buffer{};
        VkDeviceMemory memory{};
        void* mappedMemory{};
        size_t count;
        VkBufferUsageFlags usage;
        VkMemoryAllocateFlags allocFlags;

    public:

        /**
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        HostBuffer(VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice) : GPUDataStorage(device, physicalDevice), usage(usage), allocFlags(allocFlags) {
            count = 0;
        }

        /**
         * @param count Number of elements (0 is promoted to 1).
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        HostBuffer(size_t count, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice) : GPUDataStorage(device, physicalDevice), usage(usage), count(count), allocFlags(allocFlags) {
            if (count == 0) {
                count = 1;
            }         
            initBuffer(count);
        }

        /**
         * @param data CPU pointer to initial data.
         * @param count Number of elements (0 is promoted to 1).
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        HostBuffer(const T* data, size_t count, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice) : GPUDataStorage(device, physicalDevice), usage(usage), allocFlags(allocFlags) {
            if (count == 0) {
                count = 1;
            }
            initBuffer(count);
            copyToBuffer(data, count);
        }


        /** Destroy and release buffer resources. */
        ~HostBuffer() {
            destroyBuffer();
        }

        /** Destroy buffer and memory, unmapping if needed. */
        void destroyBuffer() {
            if (mappedMemory) { vkUnmapMemory(device, memory); mappedMemory = nullptr; }
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
        }

        /**
         * @param count Number of elements.
         */
        void initBuffer(size_t count) {
            this->count = count;
            VkDeviceSize bufferSize = sizeof(T) * count;

            createBuffer(bufferSize, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocFlags, buffer, memory);

            vkMapMemory(device, memory, 0, bufferSize, 0, &mappedMemory);
        }

        /**
         * @param data CPU pointer to data.
         * @param count Number of elements to copy.
         */
        void copyToBuffer(const T* data, size_t count) {
            VkDeviceSize bufferSize = sizeof(T) * count;
            memcpy(mappedMemory, data, bufferSize);
        }

        /**
         * @param data CPU pointer to data.
         * @param count Number of elements.
         */
        void updateBuffer(const T* data, size_t count) {
            if (count == 0) {
                count = 1;
            }
            VkDeviceSize bufferSize = sizeof(T) * count;
            if (count > this->count) {
                destroyBuffer();
                initBuffer(count);
            }
            copyToBuffer(data, count);
        }

        /** @return Device address of the buffer. */
        VkDeviceAddress getDeviceAddress() {
            VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
            info.buffer = buffer;
            return vkGetBufferDeviceAddress(device, &info);
        }

        /** @return Vulkan buffer handle. */
        VkBuffer getBuffer() const { return buffer; }

        /** @return Element count currently allocated. */
        size_t getCount() const { return count; }
    };

    /** Device-local, typed buffer populated via a staging copy. */
    template <typename T>
    class DeviceBuffer : public GPUDataStorage {
    private:
        VkBuffer buffer{};
        VkDeviceMemory memory{};
        size_t count;
        VkBufferUsageFlags usage;
        CommandManager* commandManager;
        VkMemoryAllocateFlags allocFlags;

        /**
         * @param srcBuffer Source buffer.
         * @param dstBuffer Destination buffer.
         * @param size Size in bytes to copy.
         */
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
            VkCommandBuffer commandBuffer = commandManager->beginSingleTimeCommand();

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0; //!< Optional
            copyRegion.dstOffset = 0; //!< Optional
            copyRegion.size = size;
            vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
            commandManager->endSingleTimeCommand(commandBuffer);
        }

    public:

        /**
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param commandManager Command manager for staging copies.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        DeviceBuffer(VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
            GPUDataStorage(device, physicalDevice), usage(usage), commandManager(commandManager), allocFlags(allocFlags) {
        }

        /**
         * @param data CPU pointer to initial data.
         * @param count Number of elements (0 is promoted to 1).
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param commandManager Command manager for staging copies.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        DeviceBuffer(const T* data, size_t count, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
            GPUDataStorage(device, physicalDevice), usage(usage), commandManager(commandManager), allocFlags(allocFlags) {
            if (count == 0) {
                count = 1;
            }
            initBuffer(data, count);
        }

        /** Destroy and release buffer resources. */
        ~DeviceBuffer() {
            destroyBuffer();
        }

        /** Destroy buffer and memory. */
        void destroyBuffer() {
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
        }

        /**
         * @param data CPU pointer to data.
         * @param count Number of elements.
         */
        void initBuffer(const T* data, size_t count) {
            this->count = count;
            VkDeviceSize bufferSize = sizeof(T) * count;

            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, stagingBuffer, stagingBufferMemory);

            void* stagingMappedMemory;
            vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &stagingMappedMemory);
            memcpy(stagingMappedMemory, data, (size_t)bufferSize);
            vkUnmapMemory(device, stagingBufferMemory);

            createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocFlags, buffer, memory);

            copyBuffer(stagingBuffer, buffer, bufferSize);

            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingBufferMemory, nullptr);
        }

        /**
         * @param data CPU pointer to data.
         * @param count Number of elements (0 is promoted to 1).
         */
        void updateBuffer(const T* data, size_t count) {
            if (count == 0) {
                count = 1;
            }
            destroyBuffer();
            initBuffer(data, count);
        }

        /** @return Vulkan buffer handle. */
        VkBuffer getBuffer() const { return buffer; }

        /** @return Element count currently allocated. */
        size_t getCount() const { return count; }

        /** @return Device address of the buffer. */
        VkDeviceAddress getDeviceAddress() {
            VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
            info.buffer = buffer;
            return vkGetBufferDeviceAddress(device, &info);
        }

    };

    /** Device-local, untyped buffer with explicit staging transfer. */
    class RawDeviceBuffer : public GPUDataStorage {
    private:
        VkBuffer buffer{};
        VkDeviceMemory memory{};
        VkDeviceSize bufferSize{};
        VkBufferUsageFlags usage;
        VkMemoryAllocateFlags allocFlags;
        CommandManager* commandManager;

        /**
         * @param srcBuffer Source buffer.
         * @param dstBuffer Destination buffer.
         * @param size Size in bytes to copy.
         */
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
            VkCommandBuffer cmd = commandManager->beginSingleTimeCommand();
            VkBufferCopy region{ 0, 0, size };
            vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &region);
            commandManager->endSingleTimeCommand(cmd);
        }

    public:
        /**
         * @param size Buffer size in bytes (0 is promoted to 1).
         * @param data CPU pointer to initial data.
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param commandManager Command manager for staging copies.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        RawDeviceBuffer(VkDeviceSize size, const void* data, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : GPUDataStorage(device, physicalDevice), usage(usage), allocFlags(allocFlags), commandManager(commandManager), bufferSize(size)
        {   
            if (size == 0) {
                size = 1;
            }
            initBuffer(size);
            transferData(data, size);
        }

        /**
         * @param size Buffer size in bytes (0 is promoted to 1).
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param commandManager Command manager for staging copies.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        RawDeviceBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : GPUDataStorage(device, physicalDevice), usage(usage), allocFlags(allocFlags), commandManager(commandManager), bufferSize(size)
        {
            if (size == 0) {
                size = 1;
            }
            initBuffer(size);
        }

        /** Destroy and release buffer resources. */
        ~RawDeviceBuffer() {
            destroyBuffer();
        }

        /** Destroy buffer and memory. */
        void destroyBuffer() {
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
        }

        /**
         * @param size Buffer size in bytes.
         */
        void initBuffer(VkDeviceSize size) {
            /// --- Create device-local buffer ---
            createBuffer(size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                allocFlags,
                buffer,
                memory);
        }

        /**
         * @param data CPU pointer to data.
         * @param size Size in bytes.
         */
        void transferData(const void* data, VkDeviceSize size)
        {
            bufferSize = size;

            /// --- Create staging buffer ---
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingMemory;
            createBuffer(size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                0,
                stagingBuffer,
                stagingMemory);

            /// Write data into staging.
            void* mapped;
            vkMapMemory(device, stagingMemory, 0, size, 0, &mapped);
            memcpy(mapped, data, size);
            vkUnmapMemory(device, stagingMemory);

            /// Copy staging into device-local buffer.
            copyBuffer(stagingBuffer, buffer, size);

            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingMemory, nullptr);
        }

        /** @return Vulkan buffer handle. */
        VkBuffer getBuffer() const { return buffer; }
        /** @return Buffer size in bytes. */
        VkDeviceSize getSize() const { return bufferSize; }

        /** @return Device address of the buffer. */
        VkDeviceAddress getDeviceAddress() {
            VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
            info.buffer = buffer;
            return vkGetBufferDeviceAddress(device, &info);
        }
    };

}
