#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

    void createDescriptorSetLayoutTargets(VkDescriptorSetLayout& descriptorSetLayout, size_t numTargets, VkDevice device)
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings;

        for (int i = 0; i < numTargets; i++) {
            VkDescriptorSetLayoutBinding imageBinding{};
            imageBinding.binding = i;
            imageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            imageBinding.descriptorCount = 1;
            imageBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;  // only raygen writes output
            imageBinding.pImmutableSamplers = nullptr;

            bindings.push_back(imageBinding);
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

    void createDescriptorPoolTargets(VkDescriptorPool& descriptorPool, size_t numTargets, VkDevice& device) {
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) * (numTargets);

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

    void createDescriptorSetsTargets(
        std::vector<VkDescriptorSet>& descriptorSets,
        VkDescriptorPool descriptorPool,
        VkDescriptorSetLayout descriptorSetLayout,
        std::vector<RenderTarget*> targets,
        VkDevice device)
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

        // Update each descriptor set
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            std::vector<VkWriteDescriptorSet> writes;

            std::vector<VkDescriptorImageInfo> imageInfos;
            imageInfos.reserve(targets.size());

            for (RenderTarget* target : targets) {
                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                imageInfo.imageView = target->getImage(i)->getImageView();
                imageInfo.sampler = VK_NULL_HANDLE; // not used
                imageInfos.push_back(imageInfo);
            }


            for (int j = 0; j < targets.size(); j++) {
                VkWriteDescriptorSet imageWrite{};
                imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                imageWrite.dstSet = descriptorSets[i];
                imageWrite.dstBinding = j;
                imageWrite.dstArrayElement = 0;
                imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                imageWrite.descriptorCount = 1;
                imageWrite.pImageInfo = &imageInfos[j];

                writes.push_back(imageWrite);
            }
            vkUpdateDescriptorSets(device,
                static_cast<uint32_t>(writes.size()), writes.data(),
                0, nullptr);
        }
    }

}