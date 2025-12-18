#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

    void createDescriptorSetLayoutRT(VkDescriptorSetLayout& descriptorSetLayout, VkDevice device)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        //Binding 0: TLAS 
        VkDescriptorSetLayoutBinding tlasBinding{};
        tlasBinding.binding = 0;
        tlasBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        tlasBinding.descriptorCount = 1;
        tlasBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
            VK_SHADER_STAGE_MISS_BIT_KHR;
        tlasBinding.pImmutableSamplers = nullptr;

        bindings.push_back(tlasBinding);

        //Binding 1: vertexBuffer
        //Binding 2: indexBuffer
        //Binding 3: instanceBuffer
        for (int i = 1; i < 4; i++) {
            VkDescriptorSetLayoutBinding layoutBinding{};
            layoutBinding.binding = i;
            layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            layoutBinding.descriptorCount = 1;
            layoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                VK_SHADER_STAGE_MISS_BIT_KHR;
            layoutBinding.pImmutableSamplers = nullptr; // Optional
            bindings.push_back(layoutBinding);
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        layoutInfo.flags = 0;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout (RT)!");
        }
    }

    void createDescriptorPoolRT(VkDescriptorPool& descriptorPool, VkDevice& device) {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * 3;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSetsRT(std::vector<VkDescriptorSet>& descriptorSets, VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout,
        VkAccelerationStructureKHR tlas, DeviceBuffer<Vertex>* vertexBuffer, DeviceBuffer<uint32_t>* indexBuffer,
        std::vector<HostBuffer<Instance>*> instanceBuffers, VkDevice device)
    {
        // Allocate descriptor sets (one per frame)
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate RT descriptor sets!");
        }

        // Prepare TLAS descriptor struct
        VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
        asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &tlas;

        VkDescriptorBufferInfo vertexBufferInfo{};
        vertexBufferInfo.buffer = vertexBuffer->getBuffer();
        vertexBufferInfo.offset = 0;
        vertexBufferInfo.range = sizeof(Vertex) * vertexBuffer->getCount();

        VkDescriptorBufferInfo indexBufferInfo{};
        indexBufferInfo.buffer = indexBuffer->getBuffer();
        indexBufferInfo.offset = 0;
        indexBufferInfo.range = sizeof(uint32_t) * indexBuffer->getCount();

        std::vector<VkDescriptorBufferInfo> instanceBufferInfos;

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo instanceBufferInfo{};
            instanceBufferInfo.buffer = instanceBuffers[i]->getBuffer();
            instanceBufferInfo.offset = 0;
            instanceBufferInfo.range = sizeof(Instance) * instanceBuffers[i]->getCount();
            instanceBufferInfos.push_back(instanceBufferInfo);
        }

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            std::vector<VkWriteDescriptorSet> writes;
            VkWriteDescriptorSet tlasWrite{};
            tlasWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            tlasWrite.dstSet = descriptorSets[i];
            tlasWrite.dstBinding = 0;
            tlasWrite.dstArrayElement = 0;
            tlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            tlasWrite.descriptorCount = 1;
            tlasWrite.pNext = &asInfo;

            writes.push_back(tlasWrite);

            VkWriteDescriptorSet vertexWrite{};
            vertexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vertexWrite.dstSet = descriptorSets[i];
            vertexWrite.dstBinding = 1;
            vertexWrite.dstArrayElement = 0;
            vertexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            vertexWrite.descriptorCount = 1;
            vertexWrite.pBufferInfo = &vertexBufferInfo;

            writes.push_back(vertexWrite);

            VkWriteDescriptorSet indexWrite{};
            indexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            indexWrite.dstSet = descriptorSets[i];
            indexWrite.dstBinding = 2;
            indexWrite.dstArrayElement = 0;
            indexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            indexWrite.descriptorCount = 1;
            indexWrite.pBufferInfo = &indexBufferInfo;

            writes.push_back(indexWrite);

            VkWriteDescriptorSet instanceWrite{};
            instanceWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            instanceWrite.dstSet = descriptorSets[i];
            instanceWrite.dstBinding = 3;
            instanceWrite.dstArrayElement = 0;
            instanceWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            instanceWrite.descriptorCount = 1;
            instanceWrite.pBufferInfo = &instanceBufferInfos[i];

            writes.push_back(instanceWrite);

            vkUpdateDescriptorSets(device,
                static_cast<uint32_t>(writes.size()), writes.data(),
                0, nullptr);
        }
    }

}