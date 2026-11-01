module;
#include <vulkan/vulkan_core.h>
#include <vk_mem_alloc.h>
#include "../shaders/simple_shared.h"
#include "simple_probe_frag_spv.h"
#include "simple_probe_integrate_spv.h"
module VVEngine.Simple.Renderer;
import std;
import VVEngine.Simple.Types;
import VVEngine.Simple.Vulkan;
import VVEngine.Simple.IrradianceProbes;

/** @file @brief Bounded direct-only raster captures, GPU L1 SH/distance moments and inside-probe inactivation. */
namespace vve::simple {
namespace {
/// @brief Cubemap basis shared with the integration shader's probeDirection.
constexpr std::array directions{Vec3{1,0,0}, Vec3{-1,0,0}, Vec3{0,1,0}, Vec3{0,-1,0}, Vec3{0,0,1}, Vec3{0,0,-1}};
static_assert(directions.size() == IrradianceProbes::faces); // Capture and shader quadrature must use the same cube basis.
/// @brief Inverts toroidal storage into the unique world cell currently inside this volume.
Vec3 probePosition(const IrradianceProbes &p, std::uint32_t index) {
    constexpr int side = VVE_PROBE_SIDE;
    Vec3 position{};
    for (const int axis : {0,1,2}) {
        const int origin = static_cast<int>(std::round(p.origin[axis] / (p.scale * IrradianceProbes::spacing)));
        const int storage = static_cast<int>(index % side);
        position[axis] = (origin + VVE_PROBE_WRAP(storage - origin)) * p.scale * IrradianceProbes::spacing;
        index /= side;
    }
    return position;
}
/// @brief Serializes atlas reuse across all submissions on the graphics queue, including earlier lighting reads.
void barrier(VkCommandBuffer cmd, const VulkanImage &image, VkImageLayout from, VkImageLayout to) {
    const VkImageMemoryBarrier b{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, .oldLayout = from, .newLayout = to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image.image, .subresourceRange = {.aspectMask = image.aspect, .levelCount = 1, .layerCount = 1}};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,nullptr,0,nullptr,1,&b);
}
}
/// @brief Uses the existing vertex/material/lighting stages; capture deliberately compiles out ambient and haze.
VkResult ForwardRenderer::createProbePipelines(std::uint32_t textures) {
    // All scene texture specialization and shadow bindings stay identical to forward rendering.
    auto result = probeFragment.create(device.device, simple_probe_frag_spv);
    if (result != VK_SUCCESS) return result;
    result = probeIntegration.create(device.device, simple_probe_integrate_spv);
    if (result != VK_SUCCESS) return result;
    // Capture is single-sample, independent of the main view coverage count.
    const VulkanVertexInputDescription input{};
    result = probePipeline.create(device.device, deferredPipelineLayout.pipelineLayout, vertShaderModule.shaderModule, "vertexMain",
        probeFragment.shaderModule, input, gbufferFormats[0], depthFormat, textures, false, VK_SAMPLE_COUNT_1_BIT, {}, "fragmentMain", false, false,
        VK_NULL_HANDLE, VK_NULL_HANDLE, "terrainDomainMain", true);
    if (result != VK_SUCCESS) return result;
    // Native engine terrain follows the capture camera using the existing tessellation stages.
    if (device.tessellationShader) {
        result = probeTerrainPipeline.create(device.device, deferredPipelineLayout.pipelineLayout, terrainVertexShader.shaderModule, "terrainVertexMain",
            probeFragment.shaderModule, input, gbufferFormats[0], depthFormat, textures, false, VK_SAMPLE_COUNT_1_BIT, {}, "fragmentMain", false, false,
            terrainHullShader.shaderModule, terrainDomainShader.shaderModule, "terrainDomainMain");
        if (result != VK_SUCCESS) return result;
    }
    // The integration triangle has no depth attachment and writes SH, validity and flattened directional moments.
    return probeIntegrationPipeline.create(device.device, deferredPipelineLayout.pipelineLayout, lightingVertexShader.shaderModule, "lightingVertexMain",
        probeIntegration.shaderModule, input, VK_FORMAT_R32G32B32A32_SFLOAT, depthFormat, textures, false, VK_SAMPLE_COUNT_1_BIT, {}, "integrateMain", true);
}
/// @brief Allocates a fixed-size volume, independent of drawable resolution; partial failure uses normal target cleanup.
VkResult ForwardRenderer::createProbeResources(WindowTarget &target) {
    const bool allocating = target.probes.radiance.image == VK_NULL_HANDLE;
    if (allocating && giGpuBytes() + probeByteBudget > giTotalByteBudget) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    for (auto cascade : std::views::iota(0U, unsigned(VVE_PROBE_CASCADES))) {
        auto &p = cascade == 0 ? target.probes : target.coarseProbes;
        p.scale = VVE_PROBE_SCALE(cascade);
        if (p.radiance.image == VK_NULL_HANDLE) {
            constexpr VkExtent2D atlas{IrradianceProbes::faces * IrradianceProbes::faceSize, IrradianceProbes::maxBudget * IrradianceProbes::faceSize};
            // Only this bounded atlas is rasterized; no scene-sized or region-sized allocation is made.
            auto r = p.radiance.create(allocator, device.device, atlas, gbufferFormats[0],
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
            if (r != VK_SUCCESS) return r;
            if (failGiAllocation()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            r = p.depth.create(allocator, device.device, atlas, depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);
            if (r != VK_SUCCESS) return r;
            if (failGiAllocation()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            // Coefficient alpha is explicit validity; reset commands clear every untouched texel.
            r = p.coefficients.create(allocator, device.device, {IrradianceProbes::atlasColumns, IrradianceProbes::count}, VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT);
            if (r != VK_SUCCESS) return r;
            if (failGiAllocation()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            // Every face needs immutable view data until its frame slot completes.
            r = p.uniforms.create(allocator, framesInFlight * IrradianceProbes::views);
            if (r != VK_SUCCESS) return r;
            if (failGiAllocation()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            // Report actual VMA allocation sizes, including implementation alignment, rather than only texel payload.
            p.gpuBytes = 0;
            for (const auto *image : {&p.radiance, &p.depth, &p.coefficients}) {
                VmaAllocationInfo info{}; vmaGetAllocationInfo(allocator, image->allocation, &info); p.gpuBytes += info.size;
            }
            for (const auto &buffer : p.uniforms.buffers) {
                VmaAllocationInfo info{}; vmaGetAllocationInfo(allocator, buffer.allocation, &info); p.gpuBytes += info.size;
            }
        }
        // Resize preserves capture descriptors and uniforms. Only startup, quality replacement or texture-layout growth allocates.
        if (!p.sets.descriptorSets.empty() && p.sets.textureCapacity == objectTextures.size()) continue;
        p.sets.cleanup();
        auto r = p.pool.create(device.device, framesInFlight * IrradianceProbes::views, descriptorSetLayout.bindings);
        if (r != VK_SUCCESS) return r;
        if (failGiAllocation()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
        r = p.sets.create(device.device, p.pool.descriptorPool, descriptorSetLayout.descriptorSetLayout,
            framesInFlight * IrradianceProbes::views, static_cast<std::uint32_t>(objectTextures.size()));
        if (r != VK_SUCCESS) return r;
        if (failGiAllocation()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    if (target.probeShadow.image == VK_NULL_HANDLE) {
        const auto r = target.probeShadow.create(allocator, device.device, 1, VVE_PROBE_SHADOW_RESOLUTION);
        if (r != VK_SUCCESS) return r;
        VmaAllocationInfo info{}; vmaGetAllocationInfo(allocator, target.probeShadow.allocation, &info);
        target.probeShadowBytes = info.size;
        if (failGiAllocation()) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    if (target.probes.gpuBytes + target.coarseProbes.gpuBytes + target.probeShadowBytes > probeByteBudget) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    return VK_SUCCESS;
}
/// @brief Snaps one toroidal volume, preserving its overlap; scale changes distances, never storage or quadrature.
void scrollProbes(IrradianceProbes &p, Vec3 eye) {
    const Scalar spacing = p.scale * IrradianceProbes::spacing;
    const auto snap = [spacing](Scalar x) { return (std::floor(x / spacing) - IrradianceProbes::side / 2) * spacing; };
    const Vec3 origin{snap(eye.x), snap(eye.y), snap(eye.z)}, previous = p.origin;
    const auto shift = subtract(origin, previous);
    p.reset |= std::max({std::abs(shift.x), std::abs(shift.y), std::abs(shift.z)}) >= IrradianceProbes::side * spacing;
    p.origin = origin;
    if (p.reset) {
        p.captured.fill(0); p.generation.fill(0); p.clearRows.reset();
    } else if (shift.x != 0 || shift.y != 0 || shift.z != 0) {
        // World cells entering the new bounds reuse rows; every overlapping row keeps its SH, moments and age.
        for (auto cell : std::views::iota(0U, IrradianceProbes::count)) {
            constexpr auto side = IrradianceProbes::side;
            const auto position = add(origin,scale(Vec3{Scalar(cell % side),Scalar((cell / side) % side),Scalar(cell / (side * side))},spacing));
            const int x = static_cast<int>(std::round(position.x / spacing));
            const int y = static_cast<int>(std::round(position.y / spacing));
            const int z = static_cast<int>(std::round(position.z / spacing));
            const auto row = VVE_PROBE_ROW(x,y,z);
            bool exposed = false;
            for (const int axis : {0,1,2}) exposed |= position[axis] < previous[axis] || position[axis] >= previous[axis] + side * spacing;
            if (exposed) { p.captured[row] = p.generation[row] = 0; p.clearRows.set(row); }
        }
    }
    p.updated = 0;
}
/// @brief Fits a bounded sun projection to the padded volume union, with upstream caster depth and texel snapping.
void prepareProbeShadow(WindowTarget &target, const FrameUniforms &frame, std::uint64_t generation) {
    const auto previous = target.probeShadowMatrix;
    const auto oldSun = target.probeSun;
    target.probeSun = -1;
    // The first lit directional light is the sun, matching physical-sky selection even without a sky dome.
    for (auto i : std::views::iota(0U,frame.activeDirectionalLightCount)) {
        if (frame.directionalLightDirections[i].w == 0) { target.probeSun = static_cast<int>(i); break; }
    }
    if (target.probeSun >= 0) {
        const auto &near = target.probes, &coarse = target.coarseProbes;
        const Scalar coarseSpacing = coarse.scale * IrradianceProbes::spacing;
        const Vec3 padding{coarseSpacing * VVE_PROBE_SHADOW_PADDING_CELLS};
        const Vec3 low = subtract(min(near.origin,coarse.origin),padding);
        const Vec3 high = add(max(add(near.origin,Vec3{(IrradianceProbes::side-1)*IrradianceProbes::spacing}),
            add(coarse.origin,Vec3{(IrradianceProbes::side-1)*coarseSpacing})),padding);
        const Vec3 center = scale(add(low,high),Scalar{.5});
        // A sphere encloses the union for every sun orientation. Quantization stabilizes its extent across cell moves.
        const Scalar radius = std::ceil(length(subtract(high,low)) * Scalar{.5});
        const Vec3 direction{frame.directionalLightDirections[target.probeSun]};
        const Scalar reach = radius + coarse.scale * IrradianceProbes::captureFar;
        const auto view = lookAt(subtract(center,scale(direction,reach)),center,detail::stableUp(direction));
        auto projection = orthoVulkan(-radius,radius,-radius,radius,ForwardRenderer::shadowNearPlane,2*reach);
        const auto shadowOrigin = multiply(multiply(projection,view),Vec4{0,0,0,1});
        const Scalar pixels = VVE_PROBE_SHADOW_RESOLUTION / Scalar{2};
        projection[3][0] += (std::round(shadowOrigin.x*pixels)-shadowOrigin.x*pixels)/pixels;
        projection[3][1] += (std::round(shadowOrigin.y*pixels)-shadowOrigin.y*pixels)/pixels;
        target.probeShadowMatrix = multiply(projection,view);
        target.probeShadowView = view; target.probeShadowProjection = projection;
    }
    // Retain pending projection changes across skipped acquisitions: preparing a matrix does not render its depth.
    target.probeShadowDirty |= !target.probeShadowInitialized || oldSun != target.probeSun || (target.probeSun >= 0 &&
        (target.probeShadowGeneration != generation || std::memcmp(&previous,&target.probeShadowMatrix,sizeof(previous)) != 0));
}
/// @brief One shared budget: near missing, coarse missing, stale near/coarse, then globally oldest capture.
VkResult ForwardRenderer::prepareProbes(WindowTarget &target, std::uint32_t frame) {
    const std::array volumes{&target.probes, &target.coarseProbes};
    const auto eye = (target.camera ? *target.camera : camera_).position.value;
    const auto &near = target.probes;
    // A whole coarse-cell move preserves its overlap. Larger jumps reset BOTH volumes, including their first-frame fallback.
    bool teleport = false;
    for (const int axis : {0,1,2}) {
        const Scalar cell = std::floor(eye[axis] / IrradianceProbes::spacing) - IrradianceProbes::side / 2;
        teleport |= std::abs(cell * IrradianceProbes::spacing - near.origin[axis]) > VVE_PROBE_SCALE(1) * IrradianceProbes::spacing;
    }
    const bool fallbackFrame = near.initialized && (teleport || (near.reset && target.coarseProbes.reset));
    if (fallbackFrame) for (auto *p : volumes) p->reset = true;
    for (auto *p : volumes) scrollProbes(*p,eye);
    prepareProbeShadow(target,frameUniforms_,probeShadowGeneration_);
    const auto budget = giSettings.enabled && giSettings.indirect_strength > 0 && !fallbackFrame
        ? (giSettings.quality == GiQuality::low ? 1U : IrradianceProbes::maxBudget) : 0U;
    std::array<std::uint32_t, VVE_PROBE_CASCADES * IrradianceProbes::count> candidates{};
    std::iota(candidates.begin(),candidates.end(),0U);
    const auto priority = [&](std::uint32_t candidate) {
        const auto cascade = candidate / IrradianceProbes::count, row = candidate % IrradianceProbes::count;
        const auto &p = *volumes[cascade];
        const int category = !p.captured[row] ? int(cascade) : p.generation[row] < probeGeneration_ ? 2 : 3;
        return std::tuple{category, category == 2 ? cascade : 0U,
            category < 3 ? lengthSquared(subtract(probePosition(p,row),eye)) : Scalar{},
            category == 3 ? p.captured[row] : std::uint64_t{}, candidate};
    };
    std::ranges::sort(candidates,{},priority);
    // The internal test baseline stops coarse captures AND sampling, retaining a literal environment fallback outside near coverage.
    unsigned selected{};
    for (const auto candidate : candidates) {
        if (selected == budget) break;
        const auto cascade = candidate / IrradianceProbes::count;
        if (cascade != 0 && !debugProbeCoarse) continue;
        auto &p = *volumes[cascade];
        p.selected[p.updated++] = candidate % IrradianceProbes::count; ++selected;
    }
    frameUniforms_.probeOrigin = {near.origin.x,near.origin.y,near.origin.z,IrradianceProbes::spacing};
    frameUniforms_.probeControl = {giSettings.enabled ? 1.0F : 0.0F,giSettings.indirect_strength,debugProbeVisibility ? 1.0F : 0.0F,giSettings.ao_enabled ? 1.0F : 0.0F};
    frameUniforms_.viewport.w = debugProbeCoarse ? VVE_PROBE_CASCADES : 1;
    // Every face has immutable uniforms in its fence-freed slot; captures never sample probe lighting.
    for (auto *volume : volumes) {
        auto &p = *volume;
        for (auto probe : std::views::iota(0U,p.updated)) {
            const auto position = probePosition(p,p.selected[probe]);
            for (auto face : std::views::iota(0U,IrradianceProbes::faces)) {
                const auto index = frame * IrradianceProbes::views + probe * IrradianceProbes::faces + face;
                auto uniform = frameUniforms_;
                uniform.view = lookAt(position,add(position,directions[face]),detail::stableUp(directions[face]));
                uniform.projection = perspectiveVulkan(static_cast<Scalar>(std::numbers::pi/2),one(),p.scale*IrradianceProbes::captureNear,p.scale*IrradianceProbes::captureFar);
                uniform.viewport = {IrradianceProbes::faceSize,IrradianceProbes::faceSize,Scalar(target.probeSun+1),0};
                uniform.probeOrigin.w = p.scale * IrradianceProbes::spacing;
                uniform.probeControl = {}; uniform.fogColorDensity.w = 0; uniform.distanceMist = {};
                // Capture does not reconstruct depth; reuse that matrix slot and one sun matrix without growing FrameUniforms.
                uniform.inverseViewProjection = target.probeShadowMatrix;
                if (target.probeSun >= 0) uniform.shadowViewProjs[kShadowMatrixDirBase + target.probeSun*kNumShadowCascades] = target.probeShadowMatrix;
                auto r = p.uniforms.update(index,uniform);
                if (r != VK_SUCCESS) return r;
                for (const auto &binding : descriptorSetLayout.bindings) {
                    const VkCopyDescriptorSet copy{.sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
                        .srcSet = target.descriptorSets.descriptorSets[frame],.srcBinding = binding.binding,
                        .dstSet = p.sets.descriptorSets[index],.dstBinding = binding.binding,.descriptorCount = binding.descriptorCount};
                    vkUpdateDescriptorSets(device.device,0,nullptr,1,&copy);
                }
                r = p.sets.writeUniformBuffer(index,p.uniforms.buffers[index].buffer,sizeof(FrameUniforms));
                if (r != VK_SUCCESS) return r;
            }
        }
    }
    return VK_SUCCESS;
}
/// @brief Captures at most two cubemaps after shadows, integrates on GPU, then publishes SH to deferred lighting.
VkResult ForwardRenderer::recordProbes(WindowTarget &target, VkCommandBuffer cmd, const std::function<VkResult(const NativeDrawContext &)> &draw) {
    if (target.probes.reset || target.coarseProbes.reset) ++target.probeResets; // Count executed resets, never repeated preparation on skipped acquisition.
    // Both atlas descriptors are statically used by integration, even when the other cascade has no captures yet.
    for (auto *p : {&target.probes,&target.coarseProbes}) {
        if (!p->initialized) barrier(cmd,p->radiance,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
    for (auto cascade : std::views::iota(0U, unsigned(VVE_PROBE_CASCADES))) {
        auto &p = cascade == 0 ? target.probes : target.coarseProbes;
        // Reset validity before lighting, while serializing against all earlier users of the atlas.
        if (p.reset) {
            barrier(cmd,p.coefficients,p.initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            const VkClearColorValue black{};
            const VkImageSubresourceRange range{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
            vkCmdClearColorImage(cmd,p.coefficients.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&black,1,&range);
            barrier(cmd,p.coefficients,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        } else if (p.clearRows.any()) {
            barrier(cmd,p.coefficients,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            const VkRenderingAttachmentInfo attachment{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = p.coefficients.imageView,.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,.storeOp = VK_ATTACHMENT_STORE_OP_STORE};
            const VkRenderingInfo scope{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,.renderArea = {.extent = p.coefficients.extent},
                .layerCount = 1,.colorAttachmentCount = 1,.pColorAttachments = &attachment};
            vkCmdBeginRendering(cmd,&scope);
            const VkClearAttachment black{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,.colorAttachment = 0,.clearValue = {}};
            // A row clear zeros validity AND distance moments before any integration or main-view sampling.
            for (auto row : std::views::iota(0U,IrradianceProbes::count)) {
                if (!p.clearRows.test(row)) continue;
                const VkClearRect rect{.rect = {.offset = {0,int(row)},.extent = {IrradianceProbes::atlasColumns,1}},.layerCount = 1};
                vkCmdClearAttachments(cmd,1,&black,1,&rect);
            }
            vkCmdEndRendering(cmd);
            barrier(cmd,p.coefficients,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        p.clearRows.reset();
        p.valid = static_cast<std::uint32_t>(std::ranges::count_if(p.captured, [](auto age) { return age != 0; }));
        p.stale = 0;
        for (auto row : std::views::iota(0U,IrradianceProbes::count)) p.stale += p.captured[row] && p.generation[row] < probeGeneration_;
        // Even disabled GI establishes descriptor-compatible layouts, without sampling untouched contents.
        if (!p.updated) {
            p.initialized = true; p.reset = false; continue;
        }
        // Previous integration reads finish before the face atlas is reused; discarded capture depth is also ordered.
        writeGpuTimestamp(target, cmd, cascade * 2);
        barrier(cmd,p.radiance,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        barrier(cmd,p.depth,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo color{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,.imageView = p.radiance.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {.color = {.float32 = {backgroundColor_.value.x, backgroundColor_.value.y, backgroundColor_.value.z, p.scale * IrradianceProbes::captureFar}}}};
        const VkRenderingAttachmentInfo depth{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,.imageView = p.depth.imageView,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue = {.depthStencil = {1,0}}};
        VkRenderingInfo info{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,.renderArea = {.extent = p.radiance.extent},.layerCount = 1,
            .colorAttachmentCount = 1,.pColorAttachments = &color,.pDepthAttachment = &depth};
        // A single rendering scope clears the atlas; disjoint viewports preserve every captured face.
        vkCmdBeginRendering(cmd,&info);
        for (auto probe : std::views::iota(0U,p.updated)) {
            const auto position = probePosition(p,p.selected[probe]);
            for (auto face : std::views::iota(0U,IrradianceProbes::faces)) {
                // Frustum tests receive this face transform, never the main view visible subset.
                constexpr auto size = IrradianceProbes::faceSize;
                const VkViewport viewport{.x = float(face * size),.y = float(probe * size),.width = float(size),.height = float(size),.minDepth = 0,.maxDepth = 1};
                const VkRect2D scissor{.offset = {int(face * size),int(probe * size)},.extent = {size,size}};
                vkCmdSetViewport(cmd,0,1,&viewport); vkCmdSetScissor(cmd,0,1,&scissor);
                vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,probePipeline.pipeline);
                const auto set = p.sets.descriptorSets[target.currentFrame * IrradianceProbes::views + probe * IrradianceProbes::faces + face];
                const std::array captureSets{set,target.deferredSet};
                vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,deferredPipelineLayout.pipelineLayout,0,2,captureSets.data(),0,nullptr);
                auto projection = perspectiveVulkan(static_cast<Scalar>(std::numbers::pi / 2),one(),p.scale*IrradianceProbes::captureNear,p.scale*IrradianceProbes::captureFar);
                const NativeDrawContext context{.pass = NativeDrawPass::probe_capture,.width = size,.height = size,
                    .viewportX = face * size,.viewportY = probe * size,
                    .view = lookAt(position,add(position,directions[face]),detail::stableUp(directions[face])),.projection = projection,
                    .faceIndex = face,.cascadeIndex = cascade,.captureFar = p.scale * IrradianceProbes::captureFar};
                if (const auto r = draw(context); r != VK_SUCCESS) { vkCmdEndRendering(cmd); return r; }
            }
        }
        vkCmdEndRendering(cmd);
        barrier(cmd,p.radiance,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        writeGpuTimestamp(target, cmd, cascade * 2 + 1);
        writeGpuTimestamp(target, cmd, 4 + cascade * 2);
        barrier(cmd,p.coefficients,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        // Load preserves every unselected row; each draw integrates one arbitrary toroidal destination.
        color.imageView = p.coefficients.imageView; color.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        info.renderArea.extent = p.coefficients.extent; info.pDepthAttachment = nullptr;
        vkCmdBeginRendering(cmd,&info);
        const VkViewport viewport{.width = float(IrradianceProbes::atlasColumns),.height = float(IrradianceProbes::count),.minDepth = 0,.maxDepth = 1};
        vkCmdSetViewport(cmd,0,1,&viewport);
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,probeIntegrationPipeline.pipeline);
        const std::array sets{target.descriptorSets.descriptorSets[target.currentFrame],target.deferredSet};
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,deferredPipelineLayout.pipelineLayout,0,2,sets.data(),0,nullptr);
        for (auto probe : std::views::iota(0U,p.updated)) {
            const auto row = p.selected[probe];
            const VkRect2D scissor{.offset = {0,int(row)},.extent = {IrradianceProbes::atlasColumns,1}};
            vkCmdSetScissor(cmd,0,1,&scissor);
            const ObjectPushConstants push{.shadowMatrixIndex = probe,.flags = cascade}; // Capture slot, not destination row.
            vkCmdPushConstants(cmd,deferredPipelineLayout.pipelineLayout,objectPushConstantStages,0,sizeof(push),&push);
            vkCmdDraw(cmd,3,1,0,0);
            p.valid += p.captured[row] == 0;
            p.stale -= p.captured[row] && p.generation[row] < probeGeneration_;
            p.captured[row] = ++target.probes.clock; p.generation[row] = probeGeneration_;
        }
        vkCmdEndRendering(cmd);
        barrier(cmd,p.coefficients,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        writeGpuTimestamp(target, cmd, 5 + cascade * 2);
        // Replacement, rather than accumulation, guarantees one-bounce steady radiance cannot grow with frame count.
        p.initialized = true; p.reset = false;
    }
    return VK_SUCCESS;
}
}
