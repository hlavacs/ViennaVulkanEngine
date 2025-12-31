#pragma once


namespace vve {
    void createDescriptorSetLayoutRT(VkDescriptorSetLayout& descriptorSetLayout, VkDevice device);
    void createDescriptorPoolRT(VkDescriptorPool& descriptorPool, VkDevice& device);
    void createDescriptorSetsRT(std::vector<VkDescriptorSet>& descriptorSets, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout,
        VkAccelerationStructureKHR tlas, DeviceBuffer<Vertex>* vertexBuffer, DeviceBuffer<uint32_t>* indexBuffer, std::vector<HostBuffer<vvh::Instance>*> instanceBuffers, DeviceBuffer<LightSource>* lightBuffer, VkDevice device);
}