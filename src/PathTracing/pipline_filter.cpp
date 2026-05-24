/**
 * @file pipeline_filter.cpp
 * @brief Compute pipeline implementation for filtering/post-processing.
 */

#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {

    VkShaderModule PipelineFilter::createShaderModule(const std::vector<char>& code, VkDevice& device) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }

    PipelineFilter::PipelineFilter(VkDevice device, VkPhysicalDevice physicalDevice, CommandManager* commandManager,
        DescriptorManager* targetsDescriptors, VkExtent2D extent, VkExtent2D workgroupSize, std::string shaderFile, VkPipelineStageFlagBits barrierStage)
        : device(device), physicalDevice(physicalDevice), commandManager(commandManager),
        targetsDescriptors(targetsDescriptors), extent(extent), workgroupSize(workgroupSize), shaderFile(shaderFile), barrierStage(barrierStage){}

    void PipelineFilter::setExtent(VkExtent2D extent) {
        this->extent = extent;
    }

    void PipelineFilter::bindRenderTarget(RenderTarget* target) {
        renderTargets.push_back(target);
    }

    void PipelineFilter::initComputePipeline()
    {
        // 1. Load the single compute shader
        auto compCode = readFile(shaderFile);
        VkShaderModule compModule = createShaderModule(compCode, device);

        VkPipelineShaderStageCreateInfo compShaderStageInfo{};
        compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compShaderStageInfo.module = compModule;
        compShaderStageInfo.pName = "main";

        // 2. Setup Pipeline Layout with just ONE Descriptor Set
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        VkDescriptorSetLayout layout = targetsDescriptors->getDescriptorLayout();
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &layout;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }

        // 3. Create the Compute Pipeline
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.stage = compShaderStageInfo;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }

        vkDestroyShaderModule(device, compModule, nullptr);
    }

    void PipelineFilter::freeResources() {
        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        // SBT buffer deletion removed
    }

    void PipelineFilter::recordCommandBuffer(int currentFrame)
    {
        VkCommandBuffer cmd = commandManager->getCommandBuffer(currentFrame);

        // Ensure images are in general layout for compute write/read
        //Read only images should be in SHADER_READ_ONLY_OPTIMAL
        for (RenderTarget* target : renderTargets) {
            target->getImage(currentFrame)->recordImageLayoutTransition(VK_IMAGE_LAYOUT_GENERAL, currentFrame);
        }

        // Bind the compute pipeline
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

        // Bind the single descriptor set
        VkDescriptorSet descriptorSet = targetsDescriptors->getDescriptorSets()[currentFrame];

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            pipelineLayout,
            0,
            1,
            &descriptorSet,
            0,
            nullptr
        );

        // Calculate dispatch groups based on image extent and your shader's local_size
        // Assuming a standard local_size_x = 16, local_size_y = 16 in your compute shader
        uint32_t groupCountX = (extent.width + (workgroupSize.width - 1)) / workgroupSize.width;
        uint32_t groupCountY = (extent.height + (workgroupSize.height -1)) / workgroupSize.height;

        // Dispatch compute workload
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

        // Barrier to ensure compute writes are finished before next stages read them
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        //needs user definable pipline barrier!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,   // Source stage: Compute
            barrierStage,  // Dest stage
            0,
            1, &barrier,
            0, nullptr,
            0, nullptr
        );
    }
}