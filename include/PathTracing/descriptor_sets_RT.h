#pragma once

/**
 * @file descriptor_sets_RT.h
 * @brief Descriptor set helpers for ray tracing resources (TLAS, geometry, lights).
 */

namespace vve {
    /**
     * Create the ray tracing descriptor set layout.
     * @param descriptorSetLayout Output layout handle.
     * @param device Logical device.
     */
    void createDescriptorSetLayoutRT(VkDescriptorSetLayout& descriptorSetLayout, VkDevice device);
    /**
     * Create the ray tracing descriptor pool.
     * @param descriptorPool Output pool handle.
     * @param device Logical device.
     */
    void createDescriptorPoolRT(VkDescriptorPool& descriptorPool, VkDevice& device);
    /**
     * Allocate and write ray tracing descriptor sets.
     * @param descriptorSets Output descriptor sets.
     * @param descriptorPool Descriptor pool.
     * @param descriptorSetLayout Layout for the sets.
     * @param tlas Top-level acceleration structure.
     * @param vertexBuffer Vertex buffer.
     * @param indexBuffer Index buffer.
     * @param instanceBuffers Instance data per frame.
     * @param lightBuffer Light buffer.
     * @param device Logical device.
     */
    void createDescriptorSetsRT(std::vector<VkDescriptorSet>& descriptorSets, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout,
        VkAccelerationStructureKHR tlas, DeviceBuffer<Vertex>* vertexBuffer, DeviceBuffer<uint32_t>* indexBuffer, std::vector<HostBuffer<vvh::Instance>*> instanceBuffers, DeviceBuffer<LightSource>* lightBuffer, VkDevice device);
}
