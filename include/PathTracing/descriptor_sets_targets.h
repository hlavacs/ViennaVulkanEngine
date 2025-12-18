#pragma once


namespace vve {
    void createDescriptorSetLayoutTargets(VkDescriptorSetLayout& descriptorSetLayout, size_t numTargets, VkDevice device);
    void createDescriptorPoolTargets(VkDescriptorPool& descriptorPool, size_t numTargets, VkDevice& device);
    void createDescriptorSetsTargets(std::vector<VkDescriptorSet>& descriptorSets, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout,
        std::vector<RenderTarget*> targets, VkDevice device);
}