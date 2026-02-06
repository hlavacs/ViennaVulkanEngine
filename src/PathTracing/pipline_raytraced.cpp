/**
 * @file pipline_raytraced.cpp
 * @brief Ray tracing pipeline implementation.
 */

#include "VHInclude.h"
#include "VEInclude.h"

namespace vve {
    VkShaderModule PiplineRaytraced::createShaderModule(const std::vector<char>& code, VkDevice& device) {
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

    void PiplineRaytraced::createShaderBindingTable(const VkRayTracingPipelineCreateInfoKHR& rtPipelineInfo)
    {
        uint32_t handleSize = m_rtProperties.shaderGroupHandleSize;
        uint32_t handleAlignment = m_rtProperties.shaderGroupHandleAlignment;
        uint32_t baseAlignment = m_rtProperties.shaderGroupBaseAlignment;
        uint32_t groupCount = rtPipelineInfo.groupCount;

        // Retrieve shader group handles
        size_t handleDataSize = handleSize * groupCount;
        m_shaderHandles.resize(handleDataSize);

        vkGetRayTracingShaderGroupHandlesKHR(device, graphicsPipeline, 0, groupCount, handleDataSize, m_shaderHandles.data());

        // Alignment helper
        auto alignUp = [](uint32_t v, uint32_t alignment) {
            return (v + alignment - 1) & ~(alignment - 1);
            };

        // Region sizes
        uint32_t raygenSize = alignUp(handleSize, handleAlignment);
        uint32_t missSize = alignUp(handleSize, handleAlignment);
        uint32_t hitSize = alignUp(handleSize, handleAlignment);

        // Region offsets
        uint32_t raygenOffset = 0;
        uint32_t missOffset = alignUp(raygenSize, baseAlignment);
        uint32_t hitOffset = alignUp(missOffset + missSize, baseAlignment);

        uint32_t sbtSize = hitOffset + hitSize;
        std::vector<uint8_t> sbtData(sbtSize);

        memcpy(sbtData.data() + raygenOffset, m_shaderHandles.data() + 0 * handleSize, handleSize);
        memcpy(sbtData.data() + missOffset, m_shaderHandles.data() + 1 * handleSize, handleSize);
        memcpy(sbtData.data() + hitOffset, m_shaderHandles.data() + 2 * handleSize, handleSize);

        shaderBindingTableBuffer = new RawDeviceBuffer(sbtSize, sbtData.data(), VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, commandManager, device, physicalDevice);

        VkDeviceAddress base = shaderBindingTableBuffer->getDeviceAddress();

        m_raygenRegion.deviceAddress = base + raygenOffset;
        m_raygenRegion.stride = raygenSize;
        m_raygenRegion.size = raygenSize;

        m_missRegion.deviceAddress = base + missOffset;
        m_missRegion.stride = missSize;
        m_missRegion.size = missSize;

        m_hitRegion.deviceAddress = base + hitOffset;
        m_hitRegion.stride = hitSize;
        m_hitRegion.size = hitSize;

        m_callableRegion = {}; // none used
    }

    void PiplineRaytraced::loadRayTracingFunctions()
    {
        vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));

        vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));

        vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
    }


    PiplineRaytraced::PiplineRaytraced(VkDevice device, VkPhysicalDevice physicalDevice, CommandManager* commandManager, VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties,
        VkDescriptorSetLayout descriptorSetLayoutGeneral, VkDescriptorSetLayout descriptorSetLayoutRT,
        VkDescriptorSetLayout descriptorSetLayoutTargets,
        std::vector<VkDescriptorSet> descriptorSetsTargets, VkExtent2D extent)
        : device(device), physicalDevice(physicalDevice), commandManager(commandManager), m_rtProperties(m_rtProperties),
        descriptorSetLayoutGeneral(descriptorSetLayoutGeneral), descriptorSetLayoutRT(descriptorSetLayoutRT),
        descriptorSetLayoutTargets(descriptorSetLayoutTargets), descriptorSetsTargets(descriptorSetsTargets), extent(extent)
    {
        loadRayTracingFunctions();
    }

    void PiplineRaytraced::setDescriptorSets(std::vector<VkDescriptorSet> descriptorSetsGeneral, std::vector<VkDescriptorSet> descriptorSetsRT) {
        this->descriptorSetsGeneral = descriptorSetsGeneral;
        this->descriptorSetsRT = descriptorSetsRT;
    }

    void PiplineRaytraced::setRenderTargetsDescriptorSets(std::vector<VkDescriptorSet> descriptorSetsTargets) {
        this->descriptorSetsTargets = descriptorSetsTargets;
    }

    void PiplineRaytraced::setExtent(VkExtent2D extent) {
        this->extent = extent;
    }


    void PiplineRaytraced::bindRenderTarget(RenderTarget* target) {
        renderTargets.push_back(target);
    }

    void PiplineRaytraced::initRayTracingPipeline()
    {
        // Creating all shaders
        enum StageIndices
        {
            eRaygen,
            eMiss,
            eClosestHit,
            eShaderGroupCount
        };
        std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
        for (auto& s : stages)
            s.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

        auto raygenCode = readFile("shaders/PathTracing/raygen.rgen.spv");
        auto missCode = readFile("shaders/PathTracing/miss.rmiss.spv");
        auto chitCode = readFile("shaders/PathTracing/closesthit.rchit.spv");

        VkShaderModule raygenModule = createShaderModule(raygenCode, device);
        VkShaderModule missModule = createShaderModule(missCode, device);
        VkShaderModule chitModule = createShaderModule(chitCode, device);

        stages[eRaygen].module = raygenModule;
        stages[eRaygen].pName = "main";
        stages[eRaygen].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        stages[eMiss].module = missModule;
        stages[eMiss].pName = "main";
        stages[eMiss].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        stages[eClosestHit].module = chitModule;
        stages[eClosestHit].pName = "main";
        stages[eClosestHit].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

        // Shader groups
        VkRayTracingShaderGroupCreateInfoKHR group{ VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR };
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;

        std::vector<VkRayTracingShaderGroupCreateInfoKHR> shader_groups;
        // Raygen
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = eRaygen;
        shader_groups.push_back(group);

        // Miss
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = eMiss;
        shader_groups.push_back(group);

        // closest hit shader
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = eClosestHit;
        shader_groups.push_back(group);

        VkPipelineLayoutCreateInfo pipeline_layout_create_info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        // Descriptor sets: one specific to ray tracing, and one shared with the rasterization pipeline
        std::array<VkDescriptorSetLayout, 3> layouts = { descriptorSetLayoutGeneral , descriptorSetLayoutRT, descriptorSetLayoutTargets };
        pipeline_layout_create_info.setLayoutCount = uint32_t(layouts.size());
        pipeline_layout_create_info.pSetLayouts = layouts.data();
        vkCreatePipelineLayout(device, &pipeline_layout_create_info, nullptr, &pipelineLayout);

        // Assemble the shader stages and recursion depth info into the ray tracing pipeline
        VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{ VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR };
        rtPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
        rtPipelineInfo.pStages = stages.data();
        rtPipelineInfo.groupCount = static_cast<uint32_t>(shader_groups.size());
        rtPipelineInfo.pGroups = shader_groups.data();
        rtPipelineInfo.maxPipelineRayRecursionDepth = std::max(3U, m_rtProperties.maxRayRecursionDepth);
        rtPipelineInfo.layout = pipelineLayout;
        vkCreateRayTracingPipelinesKHR(device, {}, {}, 1, &rtPipelineInfo, nullptr, &graphicsPipeline);

        // Create the shader binding table for this pipeline
        createShaderBindingTable(rtPipelineInfo);

        vkDestroyShaderModule(device, raygenModule, nullptr);
        vkDestroyShaderModule(device, missModule, nullptr);
        vkDestroyShaderModule(device, chitModule, nullptr);
    }

    void PiplineRaytraced::freeResources() {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        delete shaderBindingTableBuffer;
    }

    void PiplineRaytraced::recordCommandBuffer(int currentFrame)
    {
        VkCommandBuffer cmd = commandManager->getCommandBuffer(currentFrame);

        for (RenderTarget* target : renderTargets) {
            target->getImage(currentFrame)->recordImageLayoutTransition(VK_IMAGE_LAYOUT_GENERAL, currentFrame);
            //target->getImage(currentFrame)->memoryBarrier(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, currentFrame);
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, graphicsPipeline);

        std::array<VkDescriptorSet, 3> descriptorSets = { descriptorSetsGeneral[currentFrame], descriptorSetsRT[currentFrame], descriptorSetsTargets[currentFrame] };

        vkCmdBindDescriptorSets(
            cmd,
            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
            pipelineLayout,
            0,
            descriptorSets.size(),
            descriptorSets.data(),
            0,
            nullptr
        );

        vkCmdTraceRaysKHR(cmd, &m_raygenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, extent.width, extent.height, 1);

        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,   // old enum
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,           // old enum
            0,                                              // no dependency flags
            1, &barrier,                                    // memory barriers
            0, nullptr,                                     // buffer memory barriers
            0, nullptr                                      // image memory barriers
        );
    }

}
