/**
 * @file VERendererGaussian_Cubemap.cpp
 * @brief Cubemap IBL implementation for dynamic light probe extraction from gaussian environments
 *
 * Approach: Render gaussian splats to cubemap, then convolve to generate diffuse irradiance map.
 * Based on PBR IBL techniques from LearnOpenGL and Khronos PBR samples.
 */

#include "VHInclude.h"  // Includes volk.h for Vulkan types

// Suppress MSVC warning C4244 (VkDeviceSize to uint32_t conversion) in third-party library
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#endif
#include <vk_radix_sort.h>  // Include AFTER volk.h
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "VEInclude.h"

namespace vve {

/**
 * @brief Create cubemap resources for IBL light probe extraction
 * Creates environment cubemap and irradiance map
 */
void RendererGaussian::CreateCubemapResources() {
    auto& device = m_vkState().m_device;
    auto& allocator = m_vkState().m_vmaAllocator;

    // Create cubemap image (VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
    VkImageCreateInfo envImageInfo = {};
    envImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    envImageInfo.imageType = VK_IMAGE_TYPE_2D;
    envImageInfo.format = VK_FORMAT_B8G8R8A8_SRGB;  // Match swapchain format to reuse graphics pipeline
    envImageInfo.extent.width = m_cubemapResolution;
    envImageInfo.extent.height = m_cubemapResolution;
    envImageInfo.extent.depth = 1;
    envImageInfo.mipLevels = 1;
    envImageInfo.arrayLayers = 6;  // 6 faces
    envImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    envImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    envImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    envImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    envImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    envImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(allocator, &envImageInfo, &allocInfo, &m_envCubemap, &m_envCubemapAllocation, nullptr);

    // Create cubemap view (all 6 faces)
    VkImageViewCreateInfo envViewInfo = {};
    envViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    envViewInfo.image = m_envCubemap;
    envViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    envViewInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
    envViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    envViewInfo.subresourceRange.baseMipLevel = 0;
    envViewInfo.subresourceRange.levelCount = 1;
    envViewInfo.subresourceRange.baseArrayLayer = 0;
    envViewInfo.subresourceRange.layerCount = 6;

    vkCreateImageView(device, &envViewInfo, nullptr, &m_envCubemapView);

    // Create per-face views for rendering
    m_envCubemapFaceViews.resize(6);
    for (uint32_t face = 0; face < 6; ++face) {
        VkImageViewCreateInfo faceViewInfo = envViewInfo;
        faceViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        faceViewInfo.subresourceRange.baseArrayLayer = face;
        faceViewInfo.subresourceRange.layerCount = 1;

        vkCreateImageView(device, &faceViewInfo, nullptr, &m_envCubemapFaceViews[face]);
    }

    // ========================================================================
    // Irradiance Map (convolved from environment cubemap, smaller resolution)
    // ========================================================================

    VkImageCreateInfo irrImageInfo = envImageInfo;
    irrImageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;  // 16-bit HDR float (matches shader rgba16f declaration)
    irrImageInfo.extent.width = m_irradianceResolution;
    irrImageInfo.extent.height = m_irradianceResolution;
    irrImageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;  // Storage for compute shader write

    vmaCreateImage(allocator, &irrImageInfo, &allocInfo, &m_irradianceMap, &m_irradianceAllocation, nullptr);

    VkImageViewCreateInfo irrViewInfo = envViewInfo;
    irrViewInfo.image = m_irradianceMap;
    irrViewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    vkCreateImageView(device, &irrViewInfo, nullptr, &m_irradianceView);

    // ========================================================================
    // Create sampler for environment cubemap
    // ========================================================================

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    vkCreateSampler(device, &samplerInfo, nullptr, &m_envCubemapSampler);

    // ========================================================================
    // Initialize irradiance map layout to SHADER_READ_ONLY_OPTIMAL
    // ========================================================================

    // Create one-time command buffer for layout transition
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_vkState().m_queueFamilies.graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool commandPool;
    vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

    VkCommandBufferAllocateInfo allocInfo2 = {};
    allocInfo2.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo2.commandPool = commandPool;
    allocInfo2.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo2.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(device, &allocInfo2, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    // Transition irradiance map from UNDEFINED to SHADER_READ_ONLY_OPTIMAL
    VkImageMemoryBarrier irrBarrier = {};
    irrBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    irrBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    irrBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    irrBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    irrBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    irrBarrier.image = m_irradianceMap;
    irrBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    irrBarrier.subresourceRange.baseMipLevel = 0;
    irrBarrier.subresourceRange.levelCount = 1;
    irrBarrier.subresourceRange.baseArrayLayer = 0;
    irrBarrier.subresourceRange.layerCount = 6;
    irrBarrier.srcAccessMask = 0;
    irrBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmdBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &irrBarrier);

    vkEndCommandBuffer(cmdBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, m_vkState().m_queueFamilies.graphicsFamily.value(), 0, &graphicsQueue);
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    // Cleanup
    vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
    vkDestroyCommandPool(device, commandPool, nullptr);

    std::cout << "[Cubemap IBL] Created resources: env=" << m_cubemapResolution << "x" << m_cubemapResolution
              << ", irradiance=" << m_irradianceResolution << "x" << m_irradianceResolution << std::endl;
}

/**
 * @brief Destroy cubemap IBL resources
 */
void RendererGaussian::DestroyCubemapResources() {
    auto& device = m_vkState().m_device;
    auto& allocator = m_vkState().m_vmaAllocator;

    // Face views
    for (auto view : m_envCubemapFaceViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    m_envCubemapFaceViews.clear();

    // Environment cubemap
    if (m_envCubemapSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_envCubemapSampler, nullptr);
        m_envCubemapSampler = VK_NULL_HANDLE;
    }
    if (m_envCubemapView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_envCubemapView, nullptr);
        m_envCubemapView = VK_NULL_HANDLE;
    }
    if (m_envCubemap != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, m_envCubemap, m_envCubemapAllocation);
        m_envCubemap = VK_NULL_HANDLE;
        m_envCubemapAllocation = VK_NULL_HANDLE;
    }

    // Irradiance map
    if (m_irradianceView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_irradianceView, nullptr);
        m_irradianceView = VK_NULL_HANDLE;
    }
    if (m_irradianceMap != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, m_irradianceMap, m_irradianceAllocation);
        m_irradianceMap = VK_NULL_HANDLE;
        m_irradianceAllocation = VK_NULL_HANDLE;
    }
}

/**
 * @brief Render gaussian environment to cubemap from 6 camera angles
 * @param cmdBuffer Command buffer to record rendering commands
 * @param gaussianObjectIndex Index of gaussian object to render (environment splat)
 */
void RendererGaussian::RenderEnvironmentToCubemap(VkCommandBuffer cmdBuffer, size_t gaussianObjectIndex) {
    if (gaussianObjectIndex >= m_gaussianObjects.size()) {
        return;
    }

    auto& obj = m_gaussianObjects[gaussianObjectIndex];
    auto& device = m_vkState().m_device;
    auto& allocator = m_vkState().m_vmaAllocator;

    // Get object transform
    ObjectHandle objHandle = m_objectToBufferIndex[gaussianObjectIndex].first;
    auto position = m_registry.Get<Position&>(objHandle);
    auto rotation = m_registry.Get<Rotation&>(objHandle);
    auto scale = m_registry.Get<Scale&>(objHandle);

    // Cubemap camera setup: Position at gaussian object center (inside environment)
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);
    glm::vec3 cameraPosition = position();  // Camera at center of gaussian environment
    glm::uvec2 screenSize = glm::uvec2(m_cubemapResolution, m_cubemapResolution);

    // 6 view matrices for cubemap faces (+X, -X, +Y, -Y, +Z, -Z)
    // Fixed world-space directions - gaussians render with their rotation via model matrix
    glm::mat4 viewMatrices[6] = {
        glm::lookAt(cameraPosition, cameraPosition + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),   // +X
        glm::lookAt(cameraPosition, cameraPosition + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),  // -X
        glm::lookAt(cameraPosition, cameraPosition + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),    // +Y
        glm::lookAt(cameraPosition, cameraPosition + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),  // -Y
        glm::lookAt(cameraPosition, cameraPosition + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),   // +Z
        glm::lookAt(cameraPosition, cameraPosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))   // -Z
    };

    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), position())
                          * glm::mat4(rotation())
                          * glm::scale(glm::mat4(1.0f), scale());

    // Update camera uniform buffer once (shared across all 6 faces)
    struct CameraUBO {
        glm::mat4 projection;
        glm::vec3 cameraPosition;
        float pad0;
        glm::uvec2 screenSize;
    };

    CameraUBO cameraData{};
    cameraData.projection = projectionMatrix;
    cameraData.cameraPosition = cameraPosition;
    cameraData.screenSize = screenSize;

    void* data;
    vmaMapMemory(allocator, m_cameraAllocations[0], &data);
    memcpy(data, &cameraData, sizeof(CameraUBO));
    vmaUnmapMemory(allocator, m_cameraAllocations[0]);

    // Bind camera descriptor set (Set 0) - shared for all faces
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_computePipelineLayout, 0, 1,
                            &m_cameraDescriptorSets[0], 0, nullptr);

    // Bind gaussian descriptor sets (Set 1, 2, 3) - shared for all faces
    VkDescriptorSet descriptorSets[] = {
        m_gaussianDataDescriptorSets[gaussianObjectIndex],
        m_outputDescriptorSets[gaussianObjectIndex],
        m_plyDescriptorSets[gaussianObjectIndex]
    };
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_computePipelineLayout, 1, 3,
                            descriptorSets, 0, nullptr);

    // Render each cubemap face
    for (uint32_t face = 0; face < 6; ++face) {
        // Push model and view matrices for this face
        struct PushConstants {
            glm::mat4 model;
            glm::mat4 view;
        };
        PushConstants pushData{ modelMatrix, viewMatrices[face] };

        vkCmdPushConstants(cmdBuffer, m_computePipelineLayout,
                          VK_SHADER_STAGE_COMPUTE_BIT, 0,
                          sizeof(PushConstants), &pushData);

        // === Compute Pipeline: Rank, Sort, Inverse Index, Projection ===

        // Clear visible count
        vkCmdFillBuffer(cmdBuffer, obj.visibleCountBuffer, 0, sizeof(uint32_t), 0);

        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &barrier, 0, nullptr, 0, nullptr);

        // Rank shader
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_rankPipeline);
        uint32_t rankWorkgroups = (obj.pointCount + 255) / 256;
        vkCmdDispatch(cmdBuffer, rankWorkgroups, 1, 1);

        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                            0, 1, &barrier, 0, nullptr, 0, nullptr);

        // Radix sort
        vrdxCmdSortKeyValueIndirect(cmdBuffer, m_radixSorter, obj.pointCount,
                                    obj.visibleCountBuffer, 0,
                                    obj.keyBuffer, 0,
                                    obj.indexBuffer, 0,
                                    m_radixStorageBuffers[gaussianObjectIndex], 0,
                                    VK_NULL_HANDLE, 0);

        // Rebind descriptor sets after radix sort (radix sort may unbind them)
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_computePipelineLayout, 0, 1,
                                &m_cameraDescriptorSets[0], 0, nullptr);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_computePipelineLayout, 1, 3,
                                descriptorSets, 0, nullptr);

        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &barrier, 0, nullptr, 0, nullptr);

        // Inverse index
        VkDeviceSize inverseMapSize = obj.pointCount * sizeof(int32_t);
        vkCmdFillBuffer(cmdBuffer, obj.inverseMapBuffer, 0, inverseMapSize, 0xFFFFFFFF);

        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &barrier, 0, nullptr, 0, nullptr);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_inverseIndexPipeline);
        uint32_t inverseWorkgroups = (obj.pointCount + 255) / 256;
        vkCmdDispatch(cmdBuffer, inverseWorkgroups, 1, 1);

        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &barrier, 0, nullptr, 0, nullptr);

        // Projection shader
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_projectionPipeline);
        uint32_t projWorkgroups = (obj.pointCount + 255) / 256;
        vkCmdDispatch(cmdBuffer, projWorkgroups, 1, 1);

        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                            0, 1, &barrier, 0, nullptr, 0, nullptr);

        // === Graphics Pipeline: Render Splats ===

        // Transition cubemap face to COLOR_ATTACHMENT_OPTIMAL for rendering
        VkImageMemoryBarrier toAttachmentBarrier = {};
        toAttachmentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toAttachmentBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // Don't preserve previous contents
        toAttachmentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toAttachmentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toAttachmentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toAttachmentBarrier.image = m_envCubemap;
        toAttachmentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toAttachmentBarrier.subresourceRange.baseMipLevel = 0;
        toAttachmentBarrier.subresourceRange.levelCount = 1;
        toAttachmentBarrier.subresourceRange.baseArrayLayer = face;
        toAttachmentBarrier.subresourceRange.layerCount = 1;
        toAttachmentBarrier.srcAccessMask = 0;
        toAttachmentBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(cmdBuffer,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &toAttachmentBarrier);

        // Begin dynamic rendering (compatible with graphics pipeline created for dynamic rendering)
        VkRenderingAttachmentInfo colorAttachment = {};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = m_envCubemapFaceViews[face];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {{m_cubemapClearColor.r, m_cubemapClearColor.g, m_cubemapClearColor.b, 1.0f}};

        VkRenderingInfo renderingInfo = {};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = {m_cubemapResolution, m_cubemapResolution};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(cmdBuffer, &renderingInfo);

        // Set viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_cubemapResolution);
        viewport.height = static_cast<float>(m_cubemapResolution);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {m_cubemapResolution, m_cubemapResolution};
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        // Bind graphics pipeline
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_splatPipeline);

        // Bind descriptor sets for graphics (shared camera buffer and output buffer)
        VkDescriptorSet graphicsDescriptorSets[] = {
            m_cameraDescriptorSets[0],  // Set 0: Camera (shared, view in push constants)
            m_outputDescriptorSets[gaussianObjectIndex]  // Set 1: Output (instances buffer)
        };
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_graphicsPipelineLayout, 0, 2,
                                graphicsDescriptorSets, 0, nullptr);

        // Bind index buffer
        vkCmdBindIndexBuffer(cmdBuffer, obj.splatIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // Draw indexed indirect
        vkCmdDrawIndexedIndirect(cmdBuffer, obj.drawIndirectBuffer, 0, 1, 0);

        vkCmdEndRendering(cmdBuffer);

        // Transition cubemap face back to SHADER_READ_ONLY_OPTIMAL for sampling
        VkImageMemoryBarrier toShaderReadBarrier = {};
        toShaderReadBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toShaderReadBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toShaderReadBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toShaderReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShaderReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toShaderReadBarrier.image = m_envCubemap;
        toShaderReadBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toShaderReadBarrier.subresourceRange.baseMipLevel = 0;
        toShaderReadBarrier.subresourceRange.levelCount = 1;
        toShaderReadBarrier.subresourceRange.baseArrayLayer = face;
        toShaderReadBarrier.subresourceRange.layerCount = 1;
        toShaderReadBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toShaderReadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &toShaderReadBarrier);
    }
}

/**
 * @brief Generate irradiance map from environment cubemap
 * @param cmdBuffer Command buffer to record compute shader dispatch
 *
 * Selects pipeline based on m_useConvolutionFilter:
 * - TRUE: Cosine-weighted convolution (~6400 samples/pixel, slow)
 * - FALSE: Box filter (1 sample/pixel, fast)
 */
void RendererGaussian::ConvolveIrradianceMap(VkCommandBuffer cmdBuffer) {
    // Select pipeline: convolution filter (slow) or box filter (fast)
    VkPipeline pipeline = m_useConvolutionFilter ? m_irradiancePipeline : m_boxFilterPipeline;
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

    // Bind descriptor set (env cubemap + irradiance map)
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_irradiancePipelineLayout,
                            0, 1, &m_irradianceDescriptorSet, 0, nullptr);

    // Dispatch compute shader
    // Work groups: (32/8, 32/8, 6) = (4, 4, 6) = 96 work groups
    // Each work group has 8x8 threads = 64 threads per group
    uint32_t workGroupsX = (m_irradianceResolution + 7) / 8;  // Ceil division
    uint32_t workGroupsY = (m_irradianceResolution + 7) / 8;
    uint32_t workGroupsZ = 6;  // 6 cubemap faces

    vkCmdDispatch(cmdBuffer, workGroupsX, workGroupsY, workGroupsZ);
}

/**
 * @brief Generate cubemap IBL from gaussian environment (public API)
 * Creates one-time command buffer to render environment to cubemap and convolve irradiance
 * @param gaussianObjectIndex Index of gaussian object to use as environment
 */
void RendererGaussian::GenerateCubemapIBL(size_t gaussianObjectIndex) {
    if (gaussianObjectIndex >= m_gaussianObjects.size()) {
        std::cerr << "[Cubemap IBL] Error: Invalid gaussian object index " << gaussianObjectIndex << std::endl;
        return;
    }

    auto& device = m_vkState().m_device;
    auto& allocator = m_vkState().m_vmaAllocator;
    uint32_t queueFamilyIndex = m_vkState().m_queueFamilies.graphicsFamily.value();

    // Create one-time command pool
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    VkCommandPool commandPool;
    vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

    // Allocate command buffer
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer);

    // Begin recording
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    // Render gaussian environment to cubemap
    RenderEnvironmentToCubemap(cmdBuffer, gaussianObjectIndex);

    // Transition irradiance map layout for storage write
    VkImageMemoryBarrier irrBarrier = {};
    irrBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    irrBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    irrBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    irrBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    irrBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    irrBarrier.image = m_irradianceMap;
    irrBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    irrBarrier.subresourceRange.baseMipLevel = 0;
    irrBarrier.subresourceRange.levelCount = 1;
    irrBarrier.subresourceRange.baseArrayLayer = 0;
    irrBarrier.subresourceRange.layerCount = 6;
    irrBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    irrBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmdBuffer,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &irrBarrier);

    // Generate irradiance map (convolution or box filter based on toggle)
    ConvolveIrradianceMap(cmdBuffer);

    // Transition irradiance map to shader read layout for PBR sampling
    VkImageMemoryBarrier irrReadBarrier = {};
    irrReadBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    irrReadBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    irrReadBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    irrReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    irrReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    irrReadBarrier.image = m_irradianceMap;
    irrReadBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    irrReadBarrier.subresourceRange.baseMipLevel = 0;
    irrReadBarrier.subresourceRange.levelCount = 1;
    irrReadBarrier.subresourceRange.baseArrayLayer = 0;
    irrReadBarrier.subresourceRange.layerCount = 6;
    irrReadBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    irrReadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmdBuffer,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &irrReadBarrier);

    // End recording
    vkEndCommandBuffer(cmdBuffer);

    // Submit command buffer
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &graphicsQueue);
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    // Cleanup
    vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
    vkDestroyCommandPool(device, commandPool, nullptr);
}

}  // namespace vve
