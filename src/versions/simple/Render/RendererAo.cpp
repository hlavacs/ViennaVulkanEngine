module;
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include "simple_ao_frag_spv.h"
#include "simple_ao_msaa_frag_spv.h"
#include "simple_ao_filter_frag_spv.h"
#include "simple_ao_filter_msaa_frag_spv.h"
#include "simple_lighting_gi_ao_frag_spv.h"
#include "simple_lighting_gi_ao_msaa_frag_spv.h"
module VVEngine.Simple.Renderer;
import std;
import VVEngine.Simple.Vulkan;

/** @file @brief Window-local contact AO targets, spatial passes and optional fence-completed GPU timing. */
namespace vve::simple {
namespace {
/// @brief Orders previous lighting/filter reads before reuse and publishes attachment writes to subsequent fragment reads.
void aoBarrier(VkCommandBuffer cmd, const VulkanImage &image, VkImageLayout from, VkImageLayout to) {
    const bool writing = to == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const VkImageMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = from == VK_IMAGE_LAYOUT_UNDEFINED ? 0U : writing ? VK_ACCESS_SHADER_READ_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = writing ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = from, .newLayout = to, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = image.image,
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
    vkCmdPipelineBarrier(cmd, writing ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        writing ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0,nullptr,0,nullptr,1,&barrier);
}
}
/// @brief Builds two single-sample fullscreen passes while preserving multisampled G-buffer reads when requested.
VkResult ForwardRenderer::createAoPipelines(std::uint32_t textures) {
    const bool msaa = sampleCount() != VK_SAMPLE_COUNT_1_BIT;
    const std::array code{msaa ? std::span<const std::uint32_t>{simple_ao_msaa_frag_spv} : std::span<const std::uint32_t>{simple_ao_frag_spv},
        msaa ? std::span<const std::uint32_t>{simple_ao_filter_msaa_frag_spv} : std::span<const std::uint32_t>{simple_ao_filter_frag_spv}};
    constexpr std::array entries{"aoMain", "aoFilterMain"};
    // Both passes use the existing scene/G-buffer layout; only the target format and fragment entry differ.
    for (auto i : std::views::iota(0U, 2U)) {
        auto result = aoShaders[i].create(device.device, code[i]);
        if (result != VK_SUCCESS) return result;
        result = aoPipelines[i].create(device.device, deferredPipelineLayout.pipelineLayout, lightingVertexShader.shaderModule,
            "lightingVertexMain", aoShaders[i].shaderModule, VulkanVertexInputDescription{}, VK_FORMAT_R16_SFLOAT,
            depthFormat, textures, false, VK_SAMPLE_COUNT_1_BIT, {}, entries[i], true);
        if (result != VK_SUCCESS) return result;
    }
    // Preserve the exact pre-AO lighting shader when disabled, including active-probe floating-point evaluation order.
    const auto lighting = msaa ? std::span<const std::uint32_t>{simple_lighting_gi_ao_msaa_frag_spv} : std::span<const std::uint32_t>{simple_lighting_gi_ao_frag_spv};
    auto result = aoLightingShader.create(device.device, lighting);
    if (result != VK_SUCCESS) return result;
    result = aoLightingPipeline.create(device.device, deferredPipelineLayout.pipelineLayout, lightingVertexShader.shaderModule,
        "lightingVertexMain", aoLightingShader.shaderModule, VulkanVertexInputDescription{}, VK_FORMAT_R16G16B16A16_SFLOAT,
        depthFormat, textures, false, sampleCount(), {}, "lightingMain", true, msaa);
    if (result != VK_SUCCESS) return result;
    return VK_SUCCESS;
}
/// @brief Allocates two R16F images per hybrid window; runtime toggles need no replacement or descriptor mutation.
VkResult ForwardRenderer::createAoResources(WindowTarget &target) {
    if (target.aoImages[0].image != VK_NULL_HANDLE) return VK_SUCCESS;
    const auto screenBytes = std::uint64_t{target.swapchain.extent.width} * target.swapchain.extent.height * 4U;
    if (giGpuBytes(target) + screenBytes > giWindowByteBudget || giGpuBytes() + screenBytes > giTotalByteBudget) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    target.aoGpuBytes = 0;
    // Partial allocation failures release AO resources immediately; ordinary target cleanup handles other attachments.
    for (auto &image : target.aoImages) {
        const auto result = image.create(allocator, device.device, target.swapchain.extent, VK_FORMAT_R16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
        if (result != VK_SUCCESS || failGiAllocation()) {
            for (auto &partial : target.aoImages) partial.cleanup();
            target.aoGpuBytes = 0; return result != VK_SUCCESS ? result : VK_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        VmaAllocationInfo info{}; vmaGetAllocationInfo(allocator, image.allocation, &info); target.aoGpuBytes += info.size;
    }
    if (giGpuBytes(target) > giWindowByteBudget || giGpuBytes() > giTotalByteBudget) {
        for (auto &image : target.aoImages) image.cleanup();
        target.aoGpuBytes = 0; return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    return VK_SUCCESS;
}
/// @brief Runs after G-buffer publication and before lighting; queue barriers protect shared images across frame slots.
void ForwardRenderer::recordAo(WindowTarget &target, VkCommandBuffer cmd) {
    target.aoEnabled = giSettings.ao_enabled;
    // Disabled AO records no draws; establish descriptor-compatible layouts once without sampling untouched storage.
    if (!target.aoEnabled) {
        if (!target.aoInitialized) for (const auto &image : target.aoImages)
            aoBarrier(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        target.aoInitialized = true; return;
    }
    const auto frame = target.currentFrame;
    writeGpuTimestamp(target, cmd, 8);
    recordedPassOrder.push_back(RecordedPass::contact_ao);
    const std::array sets{target.descriptorSets.descriptorSets[frame], target.deferredSet};
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,deferredPipelineLayout.pipelineLayout,0,2,sets.data(),0,nullptr);
    // The raw pass writes image 0; filtering reads it and publishes image 1 to per-sample lighting.
    for (auto i : std::views::iota(0U, 2U)) {
        const auto &image = target.aoImages[i];
        aoBarrier(cmd, image, target.aoInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        const VkRenderingAttachmentInfo color{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = image.imageView, .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
        const VkRenderingInfo info{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {.extent = image.extent},
            .layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &color};
        vkCmdBeginRendering(cmd, &info);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, aoPipelines[i].pipeline);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRendering(cmd);
        aoBarrier(cmd, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    writeGpuTimestamp(target, cmd, 9);
    target.aoInitialized = true;
}
} // namespace vve::simple
