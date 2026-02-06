#pragma once

/**
 * @file descriptor_sets_targets.h
 * @brief Descriptor set helpers for render target images.
 */

namespace vve {
    /**
     * Create the descriptor set layout for render target images.
     * @param descriptorSetLayout Output layout handle.
     * @param numTargets Number of targets to bind.
     * @param device Logical device.
     */
    void createDescriptorSetLayoutTargets(VkDescriptorSetLayout& descriptorSetLayout, size_t numTargets, VkDevice device);
    /**
     * Create the descriptor pool for render target descriptors.
     * @param descriptorPool Output pool handle.
     * @param numTargets Number of targets to bind.
     * @param device Logical device.
     */
    void createDescriptorPoolTargets(VkDescriptorPool& descriptorPool, size_t numTargets, VkDevice& device);
    /**
     * Allocate and write descriptor sets for render targets.
     * @param descriptorSets Output descriptor sets.
     * @param descriptorPool Descriptor pool.
     * @param descriptorSetLayout Layout for the sets.
     * @param targets Render targets to bind.
     * @param device Logical device.
     */
    void createDescriptorSetsTargets(std::vector<VkDescriptorSet>& descriptorSets, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout,
        std::vector<RenderTarget*> targets, VkDevice device);
}
