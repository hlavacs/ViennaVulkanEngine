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
    protected:
        VkBuffer buffer{};
        VkDeviceMemory memory{};
        void* mappedMemory{};
        VkBufferUsageFlags usage;
        VkMemoryPropertyFlags properties;
        VkMemoryAllocateFlags allocFlags;
        VkDeviceSize size;

        /** Destroy buffer and memory, unmapping if needed. */
        void destroyBuffer() {
            if (mappedMemory) { vkUnmapMemory(device, memory); mappedMemory = nullptr; }
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
        }

        /** Create buffer and map it if host-visible. */
        void initBuffer(VkDeviceSize size) {
            this->size = size;
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
            initBuffer(size);
        }

        GenericBuffer(VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : GPUDataStorage(device, physicalDevice), usage(usage), size(0), properties(properties), allocFlags(allocFlags) {}

        /** Destroy and release buffer resources. */
        ~GenericBuffer() {
            destroyBuffer();
        }

        /** @return Device address of the buffer. */
        VkDeviceAddress getDeviceAddress() {
            VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
            info.buffer = buffer;
            return vkGetBufferDeviceAddress(device, &info);
        }

        /** @return Vulkan buffer handle. */
        VkBuffer getBuffer() const { return buffer; }
        VkDeviceSize getSize() const { return size; }
    };

    /** Device-local, untyped buffer with explicit staging transfer. */
    class RawDeviceBuffer : public GenericBuffer {
    protected:
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

        /**
         * @param data CPU pointer to data.
         * @param size Size in bytes.
         */
        void transferData(const void* data, VkDeviceSize size)
        {
            this->size = size;

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
            : GenericBuffer(size, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocFlags, device, physicalDevice), commandManager(commandManager)
        {
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
            : GenericBuffer(size, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocFlags, device, physicalDevice), commandManager(commandManager)
        {}

        RawDeviceBuffer(VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : GenericBuffer(usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, allocFlags, device, physicalDevice), commandManager(commandManager)
        {
        }

        /** Destroy and release buffer resources. */
        ~RawDeviceBuffer() {
            destroyBuffer();
        }

        void updateBuffer(const void* data, VkDeviceSize size) {
            if (size == 0) {
                size = 1;
            }
            destroyBuffer();
            initBuffer(size);
            transferData(data, size);
        }
    };

    /** Device-local, typed buffer populated via a staging copy. */
    template <typename T>
    class DeviceBuffer : public RawDeviceBuffer {
    private:
        size_t count;
    public:

        /**
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param commandManager Command manager for staging copies.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */

        DeviceBuffer( VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
            RawDeviceBuffer(usage, allocFlags, commandManager, device, physicalDevice), count(0) {
        }

        DeviceBuffer(size_t count, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
            RawDeviceBuffer(sizeof(T)* count, usage, allocFlags, commandManager, device, physicalDevice), count(count){
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
            RawDeviceBuffer(sizeof(T) * count, data, usage, allocFlags, commandManager, device, physicalDevice), count(count) {}

        /** Destroy and release buffer resources. */
        ~DeviceBuffer() {
            destroyBuffer();
        }

        /**
         * @param data CPU pointer to data.
         * @param count Number of elements (0 is promoted to 1).
         */
        void updateBuffer(const T* data, size_t count) {
            this->count = count;
            RawDeviceBuffer::updateBuffer(data, sizeof(T) * count);
        }

        /** @return Element count currently allocated. */
        size_t getCount() const { return count; }
    };


    /** Host-visible, typed buffer for CPU updates. */
    class RawHostBuffer : public GenericBuffer {
    private:   
        /**
         * @param data CPU pointer to data.
         * @param count Number of elements to copy.
         */
        void copyToBuffer(const void* data, VkDeviceSize size) {
            memcpy(mappedMemory, data, size);
        }

    public:

        /**
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */

        /**
         * @param count Number of elements (0 is promoted to 1).
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        RawHostBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice) 
            : GenericBuffer(size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, allocFlags, device, physicalDevice){}

        RawHostBuffer(VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : GenericBuffer(usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, allocFlags, device, physicalDevice) {
        }

        /**
         * @param data CPU pointer to initial data.
         * @param count Number of elements (0 is promoted to 1).
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        RawHostBuffer(const void* data, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice) 
            : GenericBuffer(size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, allocFlags, device, physicalDevice) {
            copyToBuffer(data, size);
        }


        /** Destroy and release buffer resources. */
        ~RawHostBuffer() {
            destroyBuffer();
        }

        /**
         * @param data CPU pointer to data.
         * @param count Number of elements.
         */
        void updateBuffer(const void* data, VkDeviceSize size) {
            if (size == 0) {
                size = 1;
            }
            if (size > this->size) {
                destroyBuffer();
                initBuffer(size);
            }
            copyToBuffer(data, size);
        }
    };


    /** Host-visible, typed buffer for CPU updates. */
    template <typename T>
    class HostBuffer : public RawHostBuffer {
    private:
        size_t count;

    public:
        /**
         * @param count Number of elements (0 is promoted to 1).
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
    
        HostBuffer(VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : RawHostBuffer(usage, allocFlags, device, physicalDevice), count(0) {
        }

        HostBuffer(size_t count, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : RawHostBuffer(sizeof(T)* count, usage, allocFlags, device, physicalDevice), count(count) {
        }

        /**
         * @param data CPU pointer to initial data.
         * @param count Number of elements (0 is promoted to 1).
         * @param usage Buffer usage flags.
         * @param allocFlags Allocation flags.
         * @param device Logical device.
         * @param physicalDevice Physical device for memory queries.
         */
        HostBuffer(const T* data, size_t count, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : RawHostBuffer(data, sizeof(T)* count, usage, allocFlags, device, physicalDevice), count(count) {
        }


        /** Destroy and release buffer resources. */
        ~HostBuffer() {
            destroyBuffer();
        }

        /**
         * @param data CPU pointer to data.
         * @param count Number of elements.
         */
        void updateBuffer(const T* data, size_t count) {
            this->count = count;
            RawHostBuffer::updateBuffer(data, sizeof(T) * count);
        }


        /** @return Element count currently allocated. */
        size_t getCount() const { return count; }
    };



}
