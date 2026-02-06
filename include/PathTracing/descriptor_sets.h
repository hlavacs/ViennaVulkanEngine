#pragma once

/**
 * @file descriptor_sets.h
 * @brief Descriptor set helpers for the general (raster/RT) pipeline resources.
 */

namespace vve {
    /**
     * Create the descriptor set layout for UBOs, materials, and textures.
     * @param descriptorSetLayout Output layout handle.
     * @param device Logical device.
     */
    void createDescriptorSetLayout(VkDescriptorSetLayout& descriptorSetLayout, VkDevice& device);
    /**
     * Create a descriptor pool sized for the general descriptor sets.
     * @param descriptorPool Output pool handle.
     * @param device Logical device.
     */
    void createDescriptorPool(VkDescriptorPool& descriptorPool, VkDevice& device);
    /**
     * Allocate and write descriptor sets for UBOs, materials, and textures.
     * @param descriptorSets Output descriptor sets.
     * @param descriptorPool Descriptor pool.
     * @param descriptorSetLayout Layout for the sets.
     * @param uniformBuffers Per-frame uniform buffers.
     * @param materialBuffer Material storage buffer.
     * @param textures Texture list to bind.
     * @param textureSampler Sampler used for texture bindings.
     * @param device Logical device.
     */
    void createDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets, VkDescriptorPool& descriptorPool, VkDescriptorSetLayout& descriptorSetLayout, std::vector<HostBuffer<UniformBufferObject>*> uniformBuffers, DeviceBuffer<Material>* materialBuffer, std::vector<Texture*> textures,
        VkSampler textureSampler, VkDevice& device);
}
