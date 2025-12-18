#pragma once

namespace vve {
    void createDescriptorSetLayout(VkDescriptorSetLayout& descriptorSetLayout, VkDevice& device);
    void createDescriptorPool(VkDescriptorPool& descriptorPool, VkDevice& device);
    void createDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets, VkDescriptorPool& descriptorPool, VkDescriptorSetLayout& descriptorSetLayout, std::vector<HostBuffer<UniformBufferObject>*> uniformBuffers, DeviceBuffer<Material>* materialBuffer, std::vector<Texture*> textures,
        VkSampler textureSampler, VkDevice& device);
}