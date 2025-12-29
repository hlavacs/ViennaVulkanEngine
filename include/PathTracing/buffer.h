#pragma once

namespace vve {
    class GPUDataStorage {

    protected:
        VkPhysicalDevice physicalDevice;
        VkDevice& device;

        GPUDataStorage(VkDevice& device, VkPhysicalDevice& physicalDevice) : device(device), physicalDevice(physicalDevice) {}

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryAllocateFlags allocFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    };

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
        GenericBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : GPUDataStorage(device, physicalDevice), usage(usage), size(size), properties(properties), allocFlags(allocFlags) {
            if (size == 0) {
                size = 1;
            }
            initBuffer();
        }

        ~GenericBuffer() {
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
        }

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

        VkDeviceAddress getDeviceAddress() {
            VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
            info.buffer = buffer;
            return vkGetBufferDeviceAddress(device, &info);
        }

        VkBuffer getBuffer() const { return buffer; }
    };

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

        HostBuffer(VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice) : GPUDataStorage(device, physicalDevice), usage(usage), allocFlags(allocFlags) {
            count = 0;
        }

        HostBuffer(size_t count, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice) : GPUDataStorage(device, physicalDevice), usage(usage), count(count), allocFlags(allocFlags) {
            if (count == 0) {
                count = 1;
            }         
            initBuffer(count);
        }

        HostBuffer(const T* data, size_t count, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, VkDevice& device, VkPhysicalDevice& physicalDevice) : GPUDataStorage(device, physicalDevice), usage(usage), allocFlags(allocFlags) {
            if (count == 0) {
                count = 1;
            }
            initBuffer(count);
            copyToBuffer(data, count);
        }


        ~HostBuffer() {
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
        }

        void initBuffer(size_t count) {
            this->count = count;
            VkDeviceSize bufferSize = sizeof(T) * count;

            createBuffer(bufferSize, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, allocFlags, buffer, memory);

            vkMapMemory(device, memory, 0, bufferSize, 0, &mappedMemory);
        }

        void copyToBuffer(const T* data, size_t count) {
            VkDeviceSize bufferSize = sizeof(T) * count;
            memcpy(mappedMemory, data, bufferSize);
        }

        void updateBuffer(const T* data, size_t count) {
            if (count == 0) {
                count = 1;
            }
            VkDeviceSize bufferSize = sizeof(T) * count;
            if (count > this->count) {
                initBuffer(count);
            }
            copyToBuffer(data, count);
        }

        VkDeviceAddress getDeviceAddress() {
            VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
            info.buffer = buffer;
            return vkGetBufferDeviceAddress(device, &info);
        }

        VkBuffer getBuffer() const { return buffer; }

        size_t getCount() const { return count; }
    };

    template <typename T>
    class DeviceBuffer : public GPUDataStorage {
    private:
        VkBuffer buffer{};
        VkDeviceMemory memory{};
        size_t count;
        VkBufferUsageFlags usage;
        CommandManager* commandManager;
        VkMemoryAllocateFlags allocFlags;

        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
            VkCommandBuffer commandBuffer = commandManager->beginSingleTimeCommand();

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0; // Optional
            copyRegion.dstOffset = 0; // Optional
            copyRegion.size = size;
            vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
            commandManager->endSingleTimeCommand(commandBuffer);
        }

    public:

        DeviceBuffer(VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
            GPUDataStorage(device, physicalDevice), usage(usage), commandManager(commandManager), allocFlags(allocFlags) {
        }

        DeviceBuffer(const T* data, size_t count, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice) :
            GPUDataStorage(device, physicalDevice), usage(usage), commandManager(commandManager), allocFlags(allocFlags) {
            if (count == 0) {
                count = 1;
            }
            initBuffer(data, count);
        }

        ~DeviceBuffer() {
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
        }

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

        void updateBuffer(const T* data, size_t count) {
            if (count == 0) {
                count = 1;
            }
            initBuffer(data, count);
        }

        VkBuffer getBuffer() const { return buffer; }

        size_t getCount() const { return count; }

        VkDeviceAddress getDeviceAddress() {
            VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
            info.buffer = buffer;
            return vkGetBufferDeviceAddress(device, &info);
        }

    };

    class RawDeviceBuffer : public GPUDataStorage {
    private:
        VkBuffer buffer{};
        VkDeviceMemory memory{};
        VkDeviceSize bufferSize{};
        VkBufferUsageFlags usage;
        VkMemoryAllocateFlags allocFlags;
        CommandManager* commandManager;

        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
            VkCommandBuffer cmd = commandManager->beginSingleTimeCommand();
            VkBufferCopy region{ 0, 0, size };
            vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &region);
            commandManager->endSingleTimeCommand(cmd);
        }

    public:
        RawDeviceBuffer(VkDeviceSize size, const void* data, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : GPUDataStorage(device, physicalDevice), usage(usage), allocFlags(allocFlags), commandManager(commandManager), bufferSize(size)
        {   
            if (size == 0) {
                size = 1;
            }
            initBuffer(size);
            transferData(data, size);
        }

        RawDeviceBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryAllocateFlags allocFlags, CommandManager* commandManager, VkDevice& device, VkPhysicalDevice& physicalDevice)
            : GPUDataStorage(device, physicalDevice), usage(usage), allocFlags(allocFlags), commandManager(commandManager), bufferSize(size)
        {
            if (size == 0) {
                size = 1;
            }
            initBuffer(size);
        }

        ~RawDeviceBuffer() {
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
        }

        void initBuffer(VkDeviceSize size) {
            // --- Create device-local buffer ---
            createBuffer(size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                allocFlags,
                buffer,
                memory);
        }

        void transferData(const void* data, VkDeviceSize size)
        {
            bufferSize = size;

            // --- Create staging buffer ---
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingMemory;
            createBuffer(size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                0,
                stagingBuffer,
                stagingMemory);

            // Write data into staging
            void* mapped;
            vkMapMemory(device, stagingMemory, 0, size, 0, &mapped);
            memcpy(mapped, data, size);
            vkUnmapMemory(device, stagingMemory);

            // copy staging device local
            copyBuffer(stagingBuffer, buffer, size);

            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingMemory, nullptr);
        }

        VkBuffer getBuffer() const { return buffer; }
        VkDeviceSize getSize() const { return bufferSize; }

        VkDeviceAddress getDeviceAddress() {
            VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
            info.buffer = buffer;
            return vkGetBufferDeviceAddress(device, &info);
        }
    };

}