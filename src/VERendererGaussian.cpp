// CRITICAL: vulkan_radix_sort must use volk's function pointers (not static Vulkan linking)
// VVE uses volk for dynamic Vulkan loading. Without VRDX_USE_VOLK, the library would
// statically link Vulkan functions, creating two separate function pointer sets causing a crash.
#define VRDX_IMPLEMENTATION  // Single-header library implementation (define ONCE in .cpp)
#define VRDX_USE_VOLK        // Use volk's dynamically loaded function pointers

#include "VHInclude.h"  // Includes volk.h - MUST come before vk_radix_sort.h

// Suppress MSVC warning C4244 (VkDeviceSize to uint32_t conversion) in third-party library
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#endif

#include <vk_radix_sort.h>  // Include AFTER volk.h with VRDX_USE_VOLK defined

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "VEInclude.h"

namespace vve {

/**
 * @brief Constructor for Gaussian renderer, registers engine callbacks
 * @param systemName Name of the renderer system
 * @param engine Engine instance to register with
 * @param windowName Name of the window for rendering
 */
RendererGaussian::RendererGaussian(const std::string& systemName, Engine& engine, const std::string& windowName)
    : Renderer(systemName, engine, windowName) {

    engine.RegisterCallbacks({
        {this,  3500, "INIT", [this](Message& message){ return OnInit(message);} },
        {this,  2000, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
        {this,  1900, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRenderNextFrame(message);} },
        {this,  1800, "OBJECT_CREATE", [this](Message& message) { return OnObjectCreate(message); } },
        {this, 10100, "OBJECT_DESTROY", [this](Message& message) { return OnObjectDestroy(message); } },
        {this,     0, "QUIT", [this](Message& message){ return OnQuit(message);} }
    });
}

/**
 * @brief Destructor for Gaussian renderer
 */
RendererGaussian::~RendererGaussian() {}

/**
 * @brief Initialize Gaussian renderer
 * @param message Initialization message
 * @return False to continue processing
 */
bool RendererGaussian::OnInit(const Message& message) {
    Renderer::OnInit(message);

    CreateComputePipelines();
    CreateGraphicsPipeline();

    CreateCameraBuffers();
    CreateCubemapResources();
    CreateDescriptorPool();
    AllocateDescriptorSets();
    UpdateDescriptorSets();

    InitializeDepthSort();

    CreateCommandBuffers();

    // Create single timestamp query pool
    // Unlike VKGS (which uses 2 pools with fences), we use vkQueueWaitIdle so 1 pool suffices
    if (m_timestampQueriesEnabled) {
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = TIMESTAMP_COUNT;

        VkQueryPool queryPool;
        if (vkCreateQueryPool(m_vkState().m_device, &queryPoolInfo, nullptr, &queryPool) == VK_SUCCESS) {
            m_timestampQueryPools.push_back(queryPool);
        }

        // Create IBL timing query pool (thesis contribution measurement)
        queryPoolInfo.queryCount = IBL_TIMESTAMP_COUNT;
        vkCreateQueryPool(m_vkState().m_device, &queryPoolInfo, nullptr, &m_iblQueryPool);

        // Get timestamp period for converting ticks to nanoseconds
        VkPhysicalDeviceProperties deviceProps;
        vkGetPhysicalDeviceProperties(m_vkState().m_physicalDevice, &deviceProps);
        m_frameTimings.timestampPeriod = deviceProps.limits.timestampPeriod;
        m_iblTimings.timestampPeriod = deviceProps.limits.timestampPeriod;
    }

    std::cout << "RendererGaussian initialized" << std::endl;

    return false;
}

/**
 * @brief Prepare Gaussian rendering for next frame
 * @param message Frame preparation message
 * @return False to continue processing
 */
bool RendererGaussian::OnPrepareNextFrame(const Message& message) {
    if (m_gaussianObjects.empty()) {
        return false;
    }

    uint32_t currentFrame = m_vkState().m_currentFrame;
    auto& swapChain = m_vkState().m_swapChain;

    // Simple synchronization: wait for GPU to finish before updating buffers
    // OPTIMIZATION: Replace with per-frame fences for proper frame-in-flight rendering:
    // vkWaitForFences(m_renderFences[currentFrame]) before updating, submit with fence signal.
    // See VKGS engine.cc:1029,1314,1325 and VERendererVulkan.cpp:280 for fence patterns.
    vkQueueWaitIdle(m_vkState().m_graphicsQueue);

    // Read timestamps from previous frame (GPU finished after vkQueueWaitIdle)
    // Reference: VKGS engine.cc:1031-1044
    // With vkQueueWaitIdle we use a single pool - safe to read then write
    if (m_timestampQueriesEnabled && !m_timestampQueryPools.empty() && m_frameTimings.valid) {
        VkQueryPool queryPool = m_timestampQueryPools[0];
        std::vector<uint64_t> timestamps(TIMESTAMP_COUNT);

        VkResult result = vkGetQueryPoolResults(
            m_vkState().m_device, queryPool,
            0, static_cast<uint32_t>(timestamps.size()),
            timestamps.size() * sizeof(uint64_t), timestamps.data(), sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
        );

        if (result == VK_SUCCESS) {
            // Calculate timings (same indexing as VKGS engine.cc:1039-1044)
            m_frameTimings.rankTime = timestamps[2] - timestamps[1];
            m_frameTimings.sortTime = timestamps[4] - timestamps[3];
            m_frameTimings.inverseIndexTime = timestamps[6] - timestamps[5];
            m_frameTimings.projectionTime = timestamps[8] - timestamps[7];
            m_frameTimings.renderTime = timestamps[10] - timestamps[9];
            m_frameTimings.totalTime = timestamps[11] - timestamps[0];
            m_frameTimings.valid = true;
        } else {
            m_frameTimings.valid = false;
        }
    }

    auto [cameraHandle, camera, LtoW] = *m_registry.GetView<vecs::Handle, Camera&, LocalToWorldMatrix&>().begin();

    // LtoW is the LocalToWorldMatrix strong type, call () to get the mat4
    glm::mat4 viewMatrix = glm::inverse(LtoW());

    glm::mat4 projectionMatrix = camera().Matrix();

    glm::vec3 cameraPosition = glm::vec3(LtoW()[3]);

    glm::uvec2 screenSize = glm::uvec2(
        swapChain.m_swapChainExtent.width,
        swapChain.m_swapChainExtent.height
    );

    // Store view matrix for push constants
    m_viewMatrix = viewMatrix;

    // Update camera uniform buffer (view matrix now pushed as constant)
    struct CameraUBO {
        glm::mat4 projection;
        glm::vec3 cameraPosition;
        float pad0;
        glm::uvec2 screenSize;
    };

    CameraUBO cameraData{};
    cameraData.projection = projectionMatrix;
    cameraData.cameraPosition = cameraPosition;
    cameraData.pad0 = 0.0f;
    cameraData.screenSize = screenSize;

    // Map and copy to GPU
    void* data;
    vmaMapMemory(m_vkState().m_vmaAllocator, m_cameraAllocations[currentFrame], &data);
    memcpy(data, &cameraData, sizeof(CameraUBO));
    vmaUnmapMemory(m_vkState().m_vmaAllocator, m_cameraAllocations[currentFrame]);

    return false;
}


/**
 * @brief Record gaussian rendering command buffer for current frame
 * @param message Render frame message
 * @return False to continue processing
 */
bool RendererGaussian::OnRenderNextFrame(const Message& message) {

    if (m_gaussianObjects.empty()) {
        return false;
    }

    auto& device = m_vkState().m_device;
    auto& swapChain = m_vkState().m_swapChain;
    uint32_t currentFrame = m_vkState().m_currentFrame;
    uint32_t imageIndex = m_vkState().m_imageIndex;
    VkCommandBuffer cmdBuffer = m_commandBuffers[currentFrame];

    // vkQueueWaitIdle() in OnPrepareNextFrame ensures GPU is idle before we update buffers
    // Simpler than per-frame buffer management, suitable for educational use

    vkResetCommandBuffer(cmdBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    // Reset and start timestamp queries (like VKGS engine.cc:1075-1076)
    // Single pool approach - safe with vkQueueWaitIdle synchronization
    VkQueryPool timestampQueryPool = VK_NULL_HANDLE;
    if (m_timestampQueriesEnabled && !m_timestampQueryPools.empty()) {
        timestampQueryPool = m_timestampQueryPools[0];
        vkCmdResetQueryPool(cmdBuffer, timestampQueryPool, 0, TIMESTAMP_COUNT);
        vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampQueryPool, 0);
    }

    // Bind camera descriptor set (Set 0)
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_computePipelineLayout, 0, 1,
                            &m_cameraDescriptorSets[currentFrame], 0, nullptr);

    for (size_t objIdx = 0; objIdx < m_gaussianObjects.size(); objIdx++) {
        auto& obj = m_gaussianObjects[objIdx];

        // Bind gaussian data descriptor sets (Set 1, Set 2, Set 3)
        VkDescriptorSet descriptorSets[] = {
            m_gaussianDataDescriptorSets[objIdx],
            m_outputDescriptorSets[objIdx],
            m_plyDescriptorSets[objIdx]
        };
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_computePipelineLayout, 1, 3,
                                descriptorSets, 0, nullptr);

        // Build model matrix from object's transform components
        ObjectHandle objHandle = m_objectToBufferIndex[objIdx].first;
        auto position = m_registry.Get<Position&>(objHandle);
        auto rotation = m_registry.Get<Rotation&>(objHandle);
        auto scale = m_registry.Get<Scale&>(objHandle);

        glm::mat4 translateMatrix = glm::translate(glm::mat4(1.0f), position());
        glm::mat4 rotationMatrix = glm::mat4(rotation());
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale());
        glm::mat4 modelMatrix = translateMatrix * rotationMatrix * scaleMatrix;

        // Push both model and view matrices
        struct PushConstants {
            glm::mat4 model;
            glm::mat4 view;
        };
        PushConstants pushData{ modelMatrix, m_viewMatrix };

        vkCmdPushConstants(cmdBuffer, m_computePipelineLayout,
                          VK_SHADER_STAGE_COMPUTE_BIT, 0,
                          sizeof(PushConstants), &pushData);

        // NOTE: parse_ply shader runs once during initialization (in AllocateGaussianDescriptorSets)
        // VKGS runs parse_ply per-frame (engine.cc:1141) for dynamic PLY loading.
        // This implementation uses static PLY data - buffers contain parsed gaussian data after initialization.

        // ============================================================================
        // RANK SHADER: Frustum culling and depth key generation
        // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:1165-1194
        // ============================================================================

        // Timestamp 1: before rank (only for first object, like VKGS)
        if (timestampQueryPool != VK_NULL_HANDLE && objIdx == 0) {
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, timestampQueryPool, 1);
        }

        // vkgs:1167 - Clear visible count buffer to 0
        // Rank shader uses atomicAdd(visible_point_count, 1) to allocate indices (rank.comp:44)
        // Starting from 0 ensures correct index allocation without accumulating across frames
        vkCmdFillBuffer(cmdBuffer, obj.visibleCountBuffer, 0, sizeof(uint32_t), 0);

        // vkgs:1169-1173 - Barrier: ensure fill completes before rank reads (use global barrier like VKGS)
        VkMemoryBarrier visibleCountBarrier{};
        visibleCountBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        visibleCountBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        visibleCountBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;  // vkgs:1171 only has READ
        vkCmdPipelineBarrier(cmdBuffer,
                            VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &visibleCountBarrier, 0, nullptr, 0, nullptr);

        // vkgs:1175 - Bind rank pipeline
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_rankPipeline);

        // vkgs:1185-1186 - Push model matrix (descriptor sets bound earlier in our impl)
        // vkgs:1190-1191 - Dispatch rank shader with workgroup calculation
        uint32_t rankWorkgroups = (obj.pointCount + 255) / 256;
        vkCmdDispatch(cmdBuffer, rankWorkgroups, 1, 1);

        // Timestamp 2: after rank
        if (timestampQueryPool != VK_NULL_HANDLE && objIdx == 0) {
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool, 2);
        }

        // NOTE: VKGS has barrier/copy here (vkgs:1196-1212) to read visible_point_count to CPU for diagnostics
        // This implementation skips CPU readback (not needed for rendering, only for VKGS's UI display)

        // CRITICAL: Barrier ensures rank shader writes complete before radix sort reads buffers
        // Rank writes: visible_point_count (via atomicAdd), key[], index[]
        // Sort reads: visible_point_count (INDIRECT mode - GPU-driven count from visibleCountBuffer)
        VkMemoryBarrier rankBarrier{};
        rankBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        rankBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        rankBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                            0, 1, &rankBarrier, 0, nullptr, 0, nullptr);

        // ============================================================================
        // RADIX SORT: Sort gaussians by depth (back-to-front for alpha blending)
        // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:1214-1222
        // Using vulkan_radix_sort library from https://github.com/jaesung-cs/vulkan_radix_sort
        // ============================================================================

        // Timestamp 3: before sort
        if (timestampQueryPool != VK_NULL_HANDLE && objIdx == 0) {
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, timestampQueryPool, 3);
        }

        // Radix sort: Back-to-front depth sorting for correct alpha blending
        vrdxCmdSortKeyValueIndirect(
            cmdBuffer,
            m_radixSorter,
            obj.pointCount,                         // maxCount (buffer capacity)
            obj.visibleCountBuffer,                 // indirectBuffer (count read from GPU)
            0,                                      // indirectOffset
            obj.keyBuffer,                          // keysBuffer (depth values)
            0,                                      // keysOffset
            obj.indexBuffer,                        // valuesBuffer (original gaussian IDs)
            0,                                      // valuesOffset
            m_radixStorageBuffers[objIdx],          // storageBuffer (temp storage)
            0,                                      // storageOffset
            VK_NULL_HANDLE,                         // queryPool (no timing)
            0                                       // query index
        );

        // Timestamp 4: after sort
        if (timestampQueryPool != VK_NULL_HANDLE && objIdx == 0) {
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool, 4);
        }

        // NOTE: Rebind ALL descriptor sets after radix sort (radix sort clears everything)
        // This is NOT in VKGS - our implementation detail due to vulkan_radix_sort behavior
        // Set 0: Camera
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_computePipelineLayout, 0, 1,
                                &m_cameraDescriptorSets[currentFrame], 0, nullptr);

        // Sets 1, 2, 3: Gaussian data
        VkDescriptorSet descriptorSetsRebind[] = {
            m_gaussianDataDescriptorSets[objIdx],
            m_outputDescriptorSets[objIdx],
            m_plyDescriptorSets[objIdx]
        };
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_computePipelineLayout, 1, 3,
                                descriptorSetsRebind, 0, nullptr);

        // CRITICAL: Barrier ensures radix sort writes complete before inverse_index reads buffers
        // Radix sort writes: keyBuffer[] (sorted depths), indexBuffer[] (sorted gaussian IDs)
        // inverse_index reads: indexBuffer[] to build inverse mapping
        // vulkan_radix_sort API requires this barrier (documented in vk_radix_sort.h)
        // VKGS combines this barrier with the fillBuffer barrier below for efficiency (engine.cc:1228-1232)
        VkMemoryBarrier globalBarrier{};
        globalBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        globalBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        globalBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmdBuffer,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &globalBarrier, 0, nullptr, 0, nullptr);

        // ============================================================================
        // INVERSE INDEX: Create mapping from original gaussian IDs to sorted positions
        // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:1224-1253
        // ============================================================================

        // Timestamp 5: before inverse index
        if (timestampQueryPool != VK_NULL_HANDLE && objIdx == 0) {
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool, 5);
        }

        // vkgs:1226 - Initialize inverse_map buffer to -1 for ALL gaussians
        // projection shader checks inverse_map[id] == -1 to skip culled gaussians (projection.comp:78-79)
        // inverse_index writes sorted positions ONLY for visible gaussians
        // Remaining entries stay -1, indicating culled (not visible)
        VkDeviceSize inverseMapSize = obj.pointCount * sizeof(int32_t);
        vkCmdFillBuffer(cmdBuffer, obj.inverseMapBuffer, 0, inverseMapSize, 0xFFFFFFFF);  // -1 in two's complement

        // vkgs:1228-1232 - Barrier: ensure fill completes before inverse_index writes (use global barrier like VKGS)
        VkMemoryBarrier inverseMapBarrier{};
        inverseMapBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        inverseMapBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        inverseMapBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmdBuffer,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &inverseMapBarrier, 0, nullptr, 0, nullptr);

        // vkgs:1234-1240 - Bind descriptor sets (done earlier, so skipping here)

        // vkgs:1242 - Bind inverse_index pipeline
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_inverseIndexPipeline);

        // vkgs:1244-1245 - Push constants (done earlier, so skipping here)

        // vkgs:1249-1250 - Dispatch based on pointCount (total gaussians), not visible_point_count
        // Launches threads for ALL gaussians, shader early-exits for culled ones (inverse_index.comp:20)
        // Alternative would require GPU-to-CPU readback of visible_point_count (expensive sync point)
        // Trade-off: Launch extra threads vs. avoid GPU-to-CPU synchronization latency
        uint32_t inverseIndexWorkgroups = (obj.pointCount + 255) / 256;
        vkCmdDispatch(cmdBuffer, inverseIndexWorkgroups, 1, 1);

        // Timestamp 6: after inverse index
        if (timestampQueryPool != VK_NULL_HANDLE && objIdx == 0) {
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool, 6);
        }

        // vkgs:1257-1261 - Barrier after inverse_index: ensure inverse_map is visible to projection
        VkMemoryBarrier inverseBarrier{};
        inverseBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        inverseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        inverseBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &inverseBarrier, 0, nullptr, 0, nullptr);

        // ============================================================================
        // PROJECTION: Transform gaussians to screen space and prepare for rendering
        // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:1255-1274
        // ============================================================================

        // Timestamp 7: before projection
        if (timestampQueryPool != VK_NULL_HANDLE && objIdx == 0) {
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool, 7);
        }

        // vkgs:1263 - Bind projection pipeline
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_projectionPipeline);

        // vkgs:1265-1266 - Push model matrix (re-push to ensure not corrupted by radix sort)
        vkCmdPushConstants(cmdBuffer, m_computePipelineLayout,
                          VK_SHADER_STAGE_COMPUTE_BIT, 0,
                          sizeof(glm::mat4), &modelMatrix);

        // vkgs:1270-1271 - Dispatch projection shader with workgroup calculation
        uint32_t projectionWorkgroups = (obj.pointCount + 255) / 256;
        vkCmdDispatch(cmdBuffer, projectionWorkgroups, 1, 1);

        // Timestamp 8: after projection
        if (timestampQueryPool != VK_NULL_HANDLE && objIdx == 0) {
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool, 8);
        }

        // vkgs:1276-1280 - Barrier: projection writes, graphics pipeline reads (draw stage)
        VkMemoryBarrier computeToGraphicsBarrier{};
        computeToGraphicsBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        computeToGraphicsBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        computeToGraphicsBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                            0, 1, &computeToGraphicsBarrier, 0, nullptr, 0, nullptr);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = swapChain.m_swapChainImageViews[imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // Load cleared framebuffer from RendererVulkan to composite onto
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea.offset = {0, 0};
        renderingInfo.renderArea.extent = swapChain.m_swapChainExtent;
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        // Timestamp 9: before rendering
        if (timestampQueryPool != VK_NULL_HANDLE && objIdx == 0) {
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool, 9);
        }

        vkCmdBeginRendering(cmdBuffer, &renderingInfo);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_splatPipeline);

        // Bind descriptor sets for graphics pipeline (VKGS binds both sets)
        // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:1433-1438
        VkDescriptorSet graphicsDescriptorSets[] = {
            m_cameraDescriptorSets[currentFrame],           // Set 0: Camera (not used by shader, but required by Vulkan)
            m_outputDescriptorSets[objIdx]                  // Set 1: Instances buffer
        };
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_graphicsPipelineLayout, 0, 2,
                                graphicsDescriptorSets, 0, nullptr);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChain.m_swapChainExtent.width);
        viewport.height = static_cast<float>(swapChain.m_swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChain.m_swapChainExtent;
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

        // Bind index buffer for TriangleList rendering (VKGS pattern)
        // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:1477-1479
        vkCmdBindIndexBuffer(cmdBuffer, obj.splatIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // Draw indexed indirect (TriangleList mode)
        // Offset 0 for indexed draw command (indexCount, instanceCount, firstIndex, vertexOffset, firstInstance)
        // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:1479
        vkCmdDrawIndexedIndirect(cmdBuffer, obj.drawIndirectBuffer, 0, 1, 0);

        vkCmdEndRendering(cmdBuffer);

        // Timestamp 10: after rendering
        if (timestampQueryPool != VK_NULL_HANDLE && objIdx == 0) {
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, timestampQueryPool, 10);
        }
    }

    // Timestamp 11: end of frame (like VKGS engine.cc:1301)
    if (timestampQueryPool != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, timestampQueryPool, 11);
        // Mark timestamps as valid for reading in next frame's OnPrepareNextFrame
        m_frameTimings.valid = true;
    }

    vkEndCommandBuffer(cmdBuffer);

    SubmitCommandBuffer(cmdBuffer);

    return false;
}

/**
 * @brief Handle Gaussian splat object creation
 * @param message Object creation message
 * @return False to continue processing
 */
bool RendererGaussian::OnObjectCreate(Message& message) {
    const ObjectHandle& oHandle = message.template GetData<MsgObjectCreate>().m_object;

    // Only process objects with GaussianSplat component
    if (!m_registry.template Has<GaussianSplat>(oHandle)) {
        return false;
    }

    auto gaussianSplat = m_registry.template Get<GaussianSplat>(oHandle);
    auto& gaussianData = gaussianSplat();  // Unwrap the strong type

    // Load PLY file
    std::cout << "Loading gaussian splat from: " << gaussianData.plyPath << std::endl;

    PLYData plyData;
    if (!GaussianAssetLoader::LoadPLY(gaussianData.plyPath, &plyData)) {
        std::cerr << "Failed to load PLY file: " << gaussianData.plyPath << std::endl;
        return false;
    }

    std::cout << "Loaded " << plyData.pointCount << " gaussian splats" << std::endl;

    // Create GPU buffers
    GaussianBuffers buffers;
    CreateGaussianBuffers(plyData, buffers);

    // Store buffers for rendering
    m_gaussianObjects.push_back(buffers);

    // Map ObjectHandle to buffer index for destruction
    size_t objectIndex = m_gaussianObjects.size() - 1;
    m_objectToBufferIndex.push_back({oHandle, objectIndex});

    // Update splat count in component (modify through strong type wrapper)
    gaussianSplat().splatCount = plyData.pointCount;

    // Allocate and update descriptor sets for this object
    AllocateGaussianDescriptorSets(objectIndex);

    std::cout << "Gaussian object created (index: " << objectIndex << ")" << std::endl;

    return false;
}

/**
 * @brief Handle Gaussian splat object destruction
 * @param message Object destruction message
 * @return False to continue processing
 */
bool RendererGaussian::OnObjectDestroy(Message& message) {
    const ObjectHandle& oHandle = message.template GetData<MsgObjectDestroy>().m_handle;

    // Only process objects with GaussianSplat component
    if (!m_registry.template Has<GaussianSplat>(oHandle)) {
        return false;
    }

    // Find buffer index from mapping (linear search)
    auto it = std::find_if(m_objectToBufferIndex.begin(), m_objectToBufferIndex.end(),
        [&oHandle](const auto& pair) { return pair.first == oHandle; });

    if (it == m_objectToBufferIndex.end()) {
        std::cerr << "Warning: Gaussian object not found in buffer index map" << std::endl;
        return false;
    }

    size_t objectIndex = it->second;

    // Wait for GPU to finish using these buffers
    vkDeviceWaitIdle(m_vkState().m_device);

    // Destroy GPU buffers for this object
    DestroyGaussianBuffers(m_gaussianObjects[objectIndex]);

    // Remove from all vectors (standard erase)
    m_gaussianObjects.erase(m_gaussianObjects.begin() + objectIndex);
    m_gaussianDataDescriptorSets.erase(m_gaussianDataDescriptorSets.begin() + objectIndex);
    m_outputDescriptorSets.erase(m_outputDescriptorSets.begin() + objectIndex);
    m_plyDescriptorSets.erase(m_plyDescriptorSets.begin() + objectIndex);
    m_radixStorageBuffers.erase(m_radixStorageBuffers.begin() + objectIndex);
    m_radixStorageAllocations.erase(m_radixStorageAllocations.begin() + objectIndex);

    // Remove from mapping
    m_objectToBufferIndex.erase(it);

    // Update indices in mapping for objects that were shifted down
    for (auto& pair : m_objectToBufferIndex) {
        if (pair.second > objectIndex) {
            pair.second--;
        }
    }

    std::cout << "Gaussian object destroyed (was at index: " << objectIndex << ")" << std::endl;

    return false;
}

/**
 * @brief Clean up Gaussian renderer resources on quit
 * @param message Quit message
 * @return False to continue processing
 */
bool RendererGaussian::OnQuit(const Message& message) {
    vkDeviceWaitIdle(m_vkState().m_device);

    // Destroy timestamp query pools
    for (auto& queryPool : m_timestampQueryPools) {
        if (queryPool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(m_vkState().m_device, queryPool, nullptr);
        }
    }
    m_timestampQueryPools.clear();

    // Destroy IBL query pool
    if (m_iblQueryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(m_vkState().m_device, m_iblQueryPool, nullptr);
        m_iblQueryPool = VK_NULL_HANDLE;
    }

    // Clean up all Gaussian objects
    for (auto& gaussianObj : m_gaussianObjects) {
        DestroyGaussianBuffers(gaussianObj);
    }
    m_gaussianObjects.clear();
    m_objectToBufferIndex.clear();

    DestroyCommandBuffers();
    DestroyPipelines();
    DestroyDescriptors();
    DestroyCubemapResources();
    DestroyCameraBuffers();
    CleanupDepthSort();

    std::cout << "RendererGaussian cleaned up" << std::endl;

    return false;
}

/**
 * @brief Create compute pipelines for gaussian processing
 */
void RendererGaussian::CreateComputePipelines() {
    auto& device = m_vkState().m_device;

    // Load pre-compiled shader modules
    auto parsePlyCode = vvh::ReadFile("shaders/GaussianSplatting/parse_ply.spv");
    auto rankCode = vvh::ReadFile("shaders/GaussianSplatting/rank.spv");
    auto inverseIndexCode = vvh::ReadFile("shaders/GaussianSplatting/inverse_index.spv");
    auto projectionCode = vvh::ReadFile("shaders/GaussianSplatting/projection.spv");

    VkShaderModuleCreateInfo shaderModuleInfo{};
    shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    shaderModuleInfo.codeSize = parsePlyCode.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(parsePlyCode.data());
    vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &m_parsePlyShader);

    shaderModuleInfo.codeSize = rankCode.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(rankCode.data());
    vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &m_rankShader);

    shaderModuleInfo.codeSize = inverseIndexCode.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(inverseIndexCode.data());
    vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &m_inverseIndexShader);

    shaderModuleInfo.codeSize = projectionCode.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(projectionCode.data());
    vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &m_projectionShader);

    // Create descriptor set layouts matching VKGS shader expectations

    // Set 0: Camera (uniform buffer)
    VkDescriptorSetLayoutBinding cameraBinding{};
    cameraBinding.binding = 0;
    cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraBinding.descriptorCount = 1;
    cameraBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo cameraLayoutInfo{};
    cameraLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    cameraLayoutInfo.pNext = nullptr;
    cameraLayoutInfo.flags = 0;
    cameraLayoutInfo.bindingCount = 1;
    cameraLayoutInfo.pBindings = &cameraBinding;
    vkCreateDescriptorSetLayout(device, &cameraLayoutInfo, nullptr, &m_cameraDescriptorLayout);

    // Set 1: Gaussian data (uniform + storage buffers) - 5 bindings total
    VkDescriptorSetLayoutBinding gaussianBindings[5] = {};

    // Binding 0: Info (point_count)
    gaussianBindings[0].binding = 0;
    gaussianBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    gaussianBindings[0].descriptorCount = 1;
    gaussianBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: GaussianPosition
    gaussianBindings[1].binding = 1;
    gaussianBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    gaussianBindings[1].descriptorCount = 1;
    gaussianBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: GaussianCov3d
    gaussianBindings[2].binding = 2;
    gaussianBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    gaussianBindings[2].descriptorCount = 1;
    gaussianBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: GaussianOpacity
    gaussianBindings[3].binding = 3;
    gaussianBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    gaussianBindings[3].descriptorCount = 1;
    gaussianBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 4: GaussianSh
    gaussianBindings[4].binding = 4;
    gaussianBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    gaussianBindings[4].descriptorCount = 1;
    gaussianBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo gaussianLayoutInfo{};
    gaussianLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    gaussianLayoutInfo.flags = 0;
    gaussianLayoutInfo.bindingCount = 5;
    gaussianLayoutInfo.pBindings = gaussianBindings;
    vkCreateDescriptorSetLayout(device, &gaussianLayoutInfo, nullptr, &m_gaussianDataDescriptorLayout);

    // Set 2: Output buffers (storage buffers) - 6 bindings total
    VkDescriptorSetLayoutBinding outputBindings[6] = {};

    // Binding 0: DrawIndirect
    outputBindings[0].binding = 0;
    outputBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    outputBindings[0].descriptorCount = 1;
    outputBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Instances
    outputBindings[1].binding = 1;
    outputBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    outputBindings[1].descriptorCount = 1;
    outputBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT;  // Used by both compute and vertex shaders

    // Binding 2: VisiblePointCount
    outputBindings[2].binding = 2;
    outputBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    outputBindings[2].descriptorCount = 1;
    outputBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: InstanceKey
    outputBindings[3].binding = 3;
    outputBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    outputBindings[3].descriptorCount = 1;
    outputBindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 4: InstanceIndex
    outputBindings[4].binding = 4;
    outputBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    outputBindings[4].descriptorCount = 1;
    outputBindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 5: InverseMap
    outputBindings[5].binding = 5;
    outputBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    outputBindings[5].descriptorCount = 1;
    outputBindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo outputLayoutInfo{};
    outputLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    outputLayoutInfo.flags = 0;
    outputLayoutInfo.bindingCount = 6;
    outputLayoutInfo.pBindings = outputBindings;
    vkCreateDescriptorSetLayout(device, &outputLayoutInfo, nullptr, &m_outputDescriptorLayout);

    // Set 3: PLY raw data (storage buffer)
    VkDescriptorSetLayoutBinding plyBinding{};
    plyBinding.binding = 0;
    plyBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    plyBinding.descriptorCount = 1;
    plyBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo plyLayoutInfo{};
    plyLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    plyLayoutInfo.flags = 0;
    plyLayoutInfo.bindingCount = 1;
    plyLayoutInfo.pBindings = &plyBinding;
    vkCreateDescriptorSetLayout(device, &plyLayoutInfo, nullptr, &m_plyDescriptorLayout);

    // Create pipeline layout with all 4 descriptor sets + push constants
    VkDescriptorSetLayout setLayouts[4] = {
        m_cameraDescriptorLayout,
        m_gaussianDataDescriptorLayout,
        m_outputDescriptorLayout,
        m_plyDescriptorLayout
    };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4) * 2; // model + view matrices (128 bytes)

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 4;
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_computePipelineLayout);

    // Create compute pipelines
    VkComputePipelineCreateInfo computePipelineInfo{};
    computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineInfo.layout = m_computePipelineLayout;

    // Parse PLY pipeline
    computePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineInfo.stage.module = m_parsePlyShader;
    computePipelineInfo.stage.pName = "main";
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_parsePlyPipeline);

    // Rank pipeline
    computePipelineInfo.stage.module = m_rankShader;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_rankPipeline);

    // Inverse index pipeline
    computePipelineInfo.stage.module = m_inverseIndexShader;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_inverseIndexPipeline);

    // Projection pipeline
    computePipelineInfo.stage.module = m_projectionShader;
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &m_projectionPipeline);

    // ========================================================================
    // Diffuse Irradiance Convolution Pipeline
    // ========================================================================
    
    auto irradianceCode = vvh::ReadFile("shaders/GaussianSplatting/convolve_irradiance.spv");
    shaderModuleInfo.codeSize = irradianceCode.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(irradianceCode.data());
    vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &m_irradianceShader);

    // Create irradiance descriptor set layout (Set 0 for irradiance shader)
    // Binding 0: Environment cubemap (sampler)
    // Binding 1: Irradiance cubemap (storage image)
    VkDescriptorSetLayoutBinding irradianceBindings[2] = {};
    irradianceBindings[0].binding = 0;
    irradianceBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    irradianceBindings[0].descriptorCount = 1;
    irradianceBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    irradianceBindings[1].binding = 1;
    irradianceBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    irradianceBindings[1].descriptorCount = 1;
    irradianceBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo irradianceLayoutInfo{};
    irradianceLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    irradianceLayoutInfo.flags = 0;
    irradianceLayoutInfo.bindingCount = 2;
    irradianceLayoutInfo.pBindings = irradianceBindings;
    vkCreateDescriptorSetLayout(device, &irradianceLayoutInfo, nullptr, &m_irradianceDescriptorLayout);

    // Create irradiance pipeline layout (single set)
    VkPipelineLayoutCreateInfo irradiancePipelineLayoutInfo{};
    irradiancePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    irradiancePipelineLayoutInfo.setLayoutCount = 1;
    irradiancePipelineLayoutInfo.pSetLayouts = &m_irradianceDescriptorLayout;
    vkCreatePipelineLayout(device, &irradiancePipelineLayoutInfo, nullptr, &m_irradiancePipelineLayout);

    // Create irradiance compute pipeline
    VkComputePipelineCreateInfo irradianceComputePipelineInfo{};
    irradianceComputePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    irradianceComputePipelineInfo.layout = m_irradiancePipelineLayout;
    irradianceComputePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    irradianceComputePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    irradianceComputePipelineInfo.stage.module = m_irradianceShader;
    irradianceComputePipelineInfo.stage.pName = "main";
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &irradianceComputePipelineInfo, nullptr, &m_irradiancePipeline);

    // ========================================================================
    // Box Filter Pipeline (fast alternative to convolution)
    // ========================================================================

    auto boxFilterCode = vvh::ReadFile("shaders/GaussianSplatting/box_filter_irradiance.spv");
    shaderModuleInfo.codeSize = boxFilterCode.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(boxFilterCode.data());
    vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &m_boxFilterShader);

    // Create box filter pipeline (shares same layout as convolution)
    VkComputePipelineCreateInfo boxFilterPipelineInfo{};
    boxFilterPipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    boxFilterPipelineInfo.layout = m_irradiancePipelineLayout;
    boxFilterPipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    boxFilterPipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    boxFilterPipelineInfo.stage.module = m_boxFilterShader;
    boxFilterPipelineInfo.stage.pName = "main";
    vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &boxFilterPipelineInfo, nullptr, &m_boxFilterPipeline);

    std::cout << "Irradiance pipelines created (2 variants: convolution, box filter)" << std::endl;
}

/**
 * @brief Create graphics pipeline for splat rendering
 */
void RendererGaussian::CreateGraphicsPipeline() {
    auto& device = m_vkState().m_device;

    // Load pre-compiled graphics shaders
    auto vertCode = vvh::ReadFile("shaders/GaussianSplatting/splat_vert.spv");
    auto fragCode = vvh::ReadFile("shaders/GaussianSplatting/splat_frag.spv");

    VkShaderModuleCreateInfo shaderModuleInfo{};
    shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

    shaderModuleInfo.codeSize = vertCode.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(vertCode.data());
    vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &m_splatVertShader);

    shaderModuleInfo.codeSize = fragCode.size();
    shaderModuleInfo.pCode = reinterpret_cast<const uint32_t*>(fragCode.data());
    vkCreateShaderModule(device, &shaderModuleInfo, nullptr, &m_splatFragShader);

    // Shader stages
    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = m_splatVertShader;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = m_splatFragShader;
    fragStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

    // Vertex input (no vertex buffers, procedurally generated in vertex shader)
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    // Input assembly (TRIANGLE_LIST for indexed quads)
    // Index buffer pattern [0,1,2,2,1,3, 4,5,6,6,5,7...] is for TRIANGLE_LIST
    // Reference: VKGS engine.cc uses TRIANGLE_LIST
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor declared as dynamic state (set per-frame via vkCmdSetViewport/Scissor)
    // Allows window resize without pipeline recreation
    // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:1426-1431
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth testing disabled (back-to-front alpha blending)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Color blending (standard alpha blending - VKGS TriangleList mode)
    // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:284-289
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;        // vkgs:284
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // vkgs:285
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;        // vkgs:287
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;  // vkgs:288
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state (viewport and scissor)
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // Graphics pipeline layout follows VKGS pattern (engine.cc:237)
    // Set 0: Camera (used by compute shaders, not graphics shader, but included for consistency)
    // Set 1: Instances buffer (used by graphics shader: splat.vert:3)
    // VKGS shares graphics_pipeline_layout_ across multiple pipelines (splat, color_line, etc.)
    // Pattern allows all pipelines to reference "camera = set 0" conceptually
    VkDescriptorSetLayout graphicsSetLayouts[] = {
        m_cameraDescriptorLayout, // Set 0: Camera (compute only, included for pattern consistency)
        m_outputDescriptorLayout  // Set 1: Instances buffer at binding 1
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = graphicsSetLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_graphicsPipelineLayout);

    // VVE uses VK_KHR_dynamic_rendering (Vulkan 1.3 core, no VkRenderPass objects needed)
    // VKGS uses traditional VkRenderPass (engine.cc:125, older pattern)
    // Dynamic rendering: Specify attachments via VkPipelineRenderingCreateInfo instead of render pass
    VkFormat colorFormat = m_vkState().m_swapChain.m_swapChainImageFormat;
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;  // VVE's depth format

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;  // Dynamic rendering info (replaces render pass)
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_graphicsPipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;  // NULL for dynamic rendering
    pipelineInfo.subpass = 0;

    vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_splatPipeline);
}

/**
 * @brief Destroy all pipelines
 */
void RendererGaussian::DestroyPipelines() {
    auto& device = m_vkState().m_device;

    // Destroy compute pipelines
    if (m_parsePlyPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_parsePlyPipeline, nullptr);
        m_parsePlyPipeline = VK_NULL_HANDLE;
    }
    if (m_rankPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_rankPipeline, nullptr);
        m_rankPipeline = VK_NULL_HANDLE;
    }
    if (m_inverseIndexPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_inverseIndexPipeline, nullptr);
        m_inverseIndexPipeline = VK_NULL_HANDLE;
    }
    if (m_projectionPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_projectionPipeline, nullptr);
        m_projectionPipeline = VK_NULL_HANDLE;
    }
    if (m_irradiancePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_irradiancePipeline, nullptr);
        m_irradiancePipeline = VK_NULL_HANDLE;
    }
    if (m_boxFilterPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_boxFilterPipeline, nullptr);
        m_boxFilterPipeline = VK_NULL_HANDLE;
    }

    // Destroy graphics pipeline
    if (m_splatPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_splatPipeline, nullptr);
        m_splatPipeline = VK_NULL_HANDLE;
    }

    // Destroy pipeline layouts
    if (m_computePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_computePipelineLayout, nullptr);
        m_computePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_irradiancePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_irradiancePipelineLayout, nullptr);
        m_irradiancePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_graphicsPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_graphicsPipelineLayout, nullptr);
        m_graphicsPipelineLayout = VK_NULL_HANDLE;
    }

    // Destroy descriptor set layouts
    if (m_cameraDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_cameraDescriptorLayout, nullptr);
        m_cameraDescriptorLayout = VK_NULL_HANDLE;
    }
    if (m_gaussianDataDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_gaussianDataDescriptorLayout, nullptr);
        m_gaussianDataDescriptorLayout = VK_NULL_HANDLE;
    }
    if (m_outputDescriptorLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_outputDescriptorLayout, nullptr);
        m_outputDescriptorLayout = VK_NULL_HANDLE;
    }

    // Destroy shader modules
    if (m_parsePlyShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_parsePlyShader, nullptr);
        m_parsePlyShader = VK_NULL_HANDLE;
    }
    if (m_rankShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_rankShader, nullptr);
        m_rankShader = VK_NULL_HANDLE;
    }
    if (m_inverseIndexShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_inverseIndexShader, nullptr);
        m_inverseIndexShader = VK_NULL_HANDLE;
    }
    if (m_projectionShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_projectionShader, nullptr);
        m_projectionShader = VK_NULL_HANDLE;
    }
    if (m_irradianceShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_irradianceShader, nullptr);
        m_irradianceShader = VK_NULL_HANDLE;
    }
    if (m_boxFilterShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_boxFilterShader, nullptr);
        m_boxFilterShader = VK_NULL_HANDLE;
    }
    if (m_splatVertShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_splatVertShader, nullptr);
        m_splatVertShader = VK_NULL_HANDLE;
    }
    if (m_splatFragShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_splatFragShader, nullptr);
        m_splatFragShader = VK_NULL_HANDLE;
    }
}

/**
 * @brief Create GPU buffers for gaussian data
 */
void RendererGaussian::CreateGaussianBuffers(const PLYData& plyData, GaussianBuffers& outBuffers) {
    outBuffers.pointCount = plyData.pointCount;
    const uint32_t N = plyData.pointCount;

    // Buffer sizes (matching VKGS format)
    // Reference: third_party/vkgs/src/vkgs/engine/splat_load_thread.cc:92 (PLY buffer)
    // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:485-491 (gaussian data buffers)
    VkDeviceSize plyBufferSize = 60 * sizeof(uint32_t) + plyData.rawData.size();  // 60 offset indices + raw PLY data
    VkDeviceSize positionBufferSize = N * 3 * sizeof(float);  // xyz per gaussian
    VkDeviceSize cov3dBufferSize = N * 6 * sizeof(float);     // 6 covariance matrix components (upper triangle)
    VkDeviceSize opacityBufferSize = N * sizeof(float);       // alpha per gaussian
    VkDeviceSize shBufferSize = N * 48 * sizeof(uint16_t);    // 48 f16 spherical harmonics coefficients

    // Pad to next multiple of 4 for radix sort library requirement
    // vulkan_radix_sort internally pads element count to multiple of 4
    uint32_t paddedN = (N + 3) & ~3u;
    VkDeviceSize keyBufferSize = paddedN * sizeof(uint32_t);
    VkDeviceSize indexBufferSize = paddedN * sizeof(uint32_t);

    auto& allocator = m_vkState().m_vmaAllocator;

    // Create PLY buffer (raw data + offsets) with staging upload
    VmaAllocationInfo allocInfo;
    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = plyBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.plyBuffer,
        .m_allocation = outBuffers.plyAllocation,
        .m_allocationInfo = &allocInfo
    });

    // Upload PLY data (offsets + raw data) via staging buffer
    // PLY buffer format: [60 uint32 offsets] + [raw point data]
    std::vector<char> plyBufferData(plyBufferSize);

    // Copy offsets (60 uint32 = 240 bytes)
    memcpy(plyBufferData.data(), plyData.offsets.data(), 60 * sizeof(uint32_t));

    // Copy raw data
    memcpy(plyBufferData.data() + 60 * sizeof(uint32_t),
           plyData.rawData.data(),
           plyData.rawData.size());

    // Create staging buffer
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = plyBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        .m_vmaFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .m_buffer = stagingBuffer,
        .m_allocation = stagingAllocation,
        .m_allocationInfo = nullptr
    });

    // Map and copy to staging buffer
    void* plyData_ptr;
    vmaMapMemory(allocator, stagingAllocation, &plyData_ptr);
    memcpy(plyData_ptr, plyBufferData.data(), plyBufferSize);
    vmaUnmapMemory(allocator, stagingAllocation);

    // Create output buffers for parse_ply compute shader
    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = positionBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.positionBuffer,
        .m_allocation = outBuffers.positionAllocation,
        .m_allocationInfo = nullptr
    });

    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = cov3dBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.cov3dBuffer,
        .m_allocation = outBuffers.cov3dAllocation,
        .m_allocationInfo = nullptr
    });

    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = opacityBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.opacityBuffer,
        .m_allocation = outBuffers.opacityAllocation,
        .m_allocationInfo = nullptr
    });

    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = shBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.shBuffer,
        .m_allocation = outBuffers.shAllocation,
        .m_allocationInfo = nullptr
    });

    // Create write buffers (updated each frame, vkQueueWaitIdle at line 80 ensures GPU idle)
    // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:436 (draw indirect: 12 uint32 = 48 bytes)
    // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:500 (instances: N * 12 float)
    VkDeviceSize drawIndirectBufferSize = 64; // DrawIndirect struct (projection.comp defines 12 uint32 = 48 bytes, rounded to 64)
    VkDeviceSize instancesBufferSize = N * 12 * sizeof(float); // N * 3 vec4 (position + rot_scale + color/opacity)
    VkDeviceSize inverseMapBufferSize = paddedN * sizeof(int32_t);  // Use paddedN for radix sort (engine.cc:497)
    VkDeviceSize visibleCountBufferSize = sizeof(uint32_t);

    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = keyBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.keyBuffer,
        .m_allocation = outBuffers.keyAllocation,
        .m_allocationInfo = nullptr
    });

    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = indexBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.indexBuffer,
        .m_allocation = outBuffers.indexAllocation,
        .m_allocationInfo = nullptr
    });

    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = drawIndirectBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.drawIndirectBuffer,
        .m_allocation = outBuffers.drawIndirectAllocation,
        .m_allocationInfo = nullptr
    });

    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = instancesBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.instancesBuffer,
        .m_allocation = outBuffers.instancesAllocation,
        .m_allocationInfo = nullptr
    });

    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = inverseMapBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.inverseMapBuffer,
        .m_allocation = outBuffers.inverseMapAllocation,
        .m_allocationInfo = nullptr
    });

    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = visibleCountBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.visibleCountBuffer,
        .m_allocation = outBuffers.visibleCountAllocation,
        .m_allocationInfo = nullptr
    });

    // Create info uniform buffer (point_count)
    VkDeviceSize infoBufferSize = sizeof(uint32_t);
    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = infoBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.infoBuffer,
        .m_allocation = outBuffers.infoAllocation,
        .m_allocationInfo = nullptr
    });

    // Upload point count to info buffer via staging
    VkBuffer infoStagingBuffer;
    VmaAllocation infoStagingAllocation;
    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = infoBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        .m_vmaFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .m_buffer = infoStagingBuffer,
        .m_allocation = infoStagingAllocation,
        .m_allocationInfo = nullptr
    });

    // Copy point count to staging buffer
    void* infoData_ptr;
    vmaMapMemory(allocator, infoStagingAllocation, &infoData_ptr);
    memcpy(infoData_ptr, &N, sizeof(uint32_t));
    vmaUnmapMemory(allocator, infoStagingAllocation);

    // Copy both staging buffers to device-local buffers via command buffer
    VkCommandBuffer cmdBuffer = m_commandBuffers[0];  // Use first command buffer for setup
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    // Copy PLY staging to PLY buffer
    VkBufferCopy plyRegion{};
    plyRegion.srcOffset = 0;
    plyRegion.dstOffset = 0;
    plyRegion.size = plyBufferSize;
    vkCmdCopyBuffer(cmdBuffer, stagingBuffer, outBuffers.plyBuffer, 1, &plyRegion);

    // Copy Info staging to Info buffer
    VkBufferCopy infoRegion{};
    infoRegion.srcOffset = 0;
    infoRegion.dstOffset = 0;
    infoRegion.size = infoBufferSize;
    vkCmdCopyBuffer(cmdBuffer, infoStagingBuffer, outBuffers.infoBuffer, 1, &infoRegion);

    // Create splat index buffer for TriangleList rendering (VKGS pattern)
    // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:610-615
    // Pattern: For each splat i, indices are [4i+0, 4i+1, 4i+2, 4i+2, 4i+1, 4i+3]
    std::vector<uint32_t> splatIndices;
    splatIndices.reserve(N * 6);  // 6 indices per splat (2 triangles)
    for (uint32_t i = 0; i < N; i++) {
        splatIndices.push_back(4 * i + 0);
        splatIndices.push_back(4 * i + 1);
        splatIndices.push_back(4 * i + 2);
        splatIndices.push_back(4 * i + 2);
        splatIndices.push_back(4 * i + 1);
        splatIndices.push_back(4 * i + 3);
    }

    VkDeviceSize splatIndexBufferSize = splatIndices.size() * sizeof(uint32_t);
    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = splatIndexBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .m_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        .m_vmaFlags = 0,
        .m_buffer = outBuffers.splatIndexBuffer,
        .m_allocation = outBuffers.splatIndexAllocation,
        .m_allocationInfo = nullptr
    });

    // Create staging buffer for index data
    VkBuffer indexStagingBuffer;
    VmaAllocation indexStagingAllocation;
    vvh::BufCreateBuffer({
        .m_vmaAllocator = allocator,
        .m_size = splatIndexBufferSize,
        .m_usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .m_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        .m_vmaFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .m_buffer = indexStagingBuffer,
        .m_allocation = indexStagingAllocation,
        .m_allocationInfo = nullptr
    });

    // Copy indices to staging buffer
    void* indexData_ptr;
    vmaMapMemory(allocator, indexStagingAllocation, &indexData_ptr);
    memcpy(indexData_ptr, splatIndices.data(), splatIndexBufferSize);
    vmaUnmapMemory(allocator, indexStagingAllocation);

    // Copy staging to device-local buffer
    VkBufferCopy indexRegion{};
    indexRegion.srcOffset = 0;
    indexRegion.dstOffset = 0;
    indexRegion.size = splatIndexBufferSize;
    vkCmdCopyBuffer(cmdBuffer, indexStagingBuffer, outBuffers.splatIndexBuffer, 1, &indexRegion);

    vkEndCommandBuffer(cmdBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    auto& graphicsQueue = m_vkState().m_graphicsQueue;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    // Cleanup staging buffers
    vvh::BufDestroyBuffer({
        .m_device = m_vkState().m_device,
        .m_vmaAllocator = allocator,
        .m_buffer = stagingBuffer,
        .m_allocation = stagingAllocation
    });

    vvh::BufDestroyBuffer({
        .m_device = m_vkState().m_device,
        .m_vmaAllocator = allocator,
        .m_buffer = infoStagingBuffer,
        .m_allocation = infoStagingAllocation
    });

    vvh::BufDestroyBuffer({
        .m_device = m_vkState().m_device,
        .m_vmaAllocator = allocator,
        .m_buffer = indexStagingBuffer,
        .m_allocation = indexStagingAllocation
    });
}

/**
 * @brief Destroy gaussian buffers
 */
void RendererGaussian::DestroyGaussianBuffers(GaussianBuffers& buffers) {
    auto& device = m_vkState().m_device;
    auto& allocator = m_vkState().m_vmaAllocator;

    if (buffers.plyBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.plyBuffer, .m_allocation = buffers.plyAllocation });
    }
    if (buffers.positionBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.positionBuffer, .m_allocation = buffers.positionAllocation });
    }
    if (buffers.cov3dBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.cov3dBuffer, .m_allocation = buffers.cov3dAllocation });
    }
    if (buffers.opacityBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.opacityBuffer, .m_allocation = buffers.opacityAllocation });
    }
    if (buffers.shBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.shBuffer, .m_allocation = buffers.shAllocation });
    }
    // Destroy write buffers
    if (buffers.keyBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.keyBuffer, .m_allocation = buffers.keyAllocation });
    }
    if (buffers.indexBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.indexBuffer, .m_allocation = buffers.indexAllocation });
    }
    if (buffers.drawIndirectBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.drawIndirectBuffer, .m_allocation = buffers.drawIndirectAllocation });
    }
    if (buffers.instancesBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.instancesBuffer, .m_allocation = buffers.instancesAllocation });
    }
    if (buffers.inverseMapBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.inverseMapBuffer, .m_allocation = buffers.inverseMapAllocation });
    }
    if (buffers.visibleCountBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.visibleCountBuffer, .m_allocation = buffers.visibleCountAllocation });
    }
    if (buffers.infoBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.infoBuffer, .m_allocation = buffers.infoAllocation });
    }
    if (buffers.splatIndexBuffer != VK_NULL_HANDLE) {
        vvh::BufDestroyBuffer({ .m_device = device, .m_vmaAllocator = allocator,
                                .m_buffer = buffers.splatIndexBuffer, .m_allocation = buffers.splatIndexAllocation });
    }

    // Reset handles
    buffers = GaussianBuffers{};
}

/**
 * @brief Create camera uniform buffers (one per frame-in-flight)
 *
 * Frame-in-flight pattern: Each frame gets its own buffer to avoid GPU/CPU race conditions
 * when multiple frames render concurrently (GPU rendering frame N while CPU prepares N+1).
 * Currently using vkQueueWaitIdle synchronization (line 80), but per-frame buffers follow
 * standard Vulkan pattern and allow future optimization to proper frame-in-flight rendering.
 */
void RendererGaussian::CreateCameraBuffers() {
    auto& allocator = m_vkState().m_vmaAllocator;
    uint32_t frameCount = static_cast<uint32_t>(m_vkState().m_swapChain.m_swapChainImages.size());

    m_cameraBuffers.resize(frameCount);
    m_cameraAllocations.resize(frameCount);

    // Camera uniform buffer (modified from VKGS - view matrix moved to push constants)
    // VKGS: projection + view + camera_position + pad + screen_size
    // This project:  projection + camera_position + pad + screen_size (see projection.comp:6-11, rank.comp:9-14)
    VkDeviceSize cameraBufferSize = sizeof(glm::mat4) + sizeof(glm::vec3) + sizeof(float) + sizeof(glm::uvec2);

    for (uint32_t i = 0; i < frameCount; i++) {
        VmaAllocationInfo allocInfo;
        vvh::BufCreateBuffer({
            .m_vmaAllocator = allocator,
            .m_size = cameraBufferSize,
            .m_usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .m_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            .m_vmaFlags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .m_buffer = m_cameraBuffers[i],
            .m_allocation = m_cameraAllocations[i],
            .m_allocationInfo = &allocInfo
        });
    }
}

/**
 * @brief Destroy camera uniform buffers
 */
void RendererGaussian::DestroyCameraBuffers() {
    auto& device = m_vkState().m_device;
    auto& allocator = m_vkState().m_vmaAllocator;

    for (size_t i = 0; i < m_cameraBuffers.size(); i++) {
        if (m_cameraBuffers[i] != VK_NULL_HANDLE) {
            vvh::BufDestroyBuffer({
                .m_device = device,
                .m_vmaAllocator = allocator,
                .m_buffer = m_cameraBuffers[i],
                .m_allocation = m_cameraAllocations[i]
            });
        }
    }

    m_cameraBuffers.clear();
    m_cameraAllocations.clear();
}

/**
 * @brief Initialize depth sort pipeline for gaussian rendering
 */
void RendererGaussian::InitializeDepthSort() {
    // Verify volk loaded VK_KHR_push_descriptor extension (required by vulkan_radix_sort internally)
    if (vkCmdPushDescriptorSetKHR == nullptr) {
        std::cerr << "ERROR: vkCmdPushDescriptorSetKHR is NULL! Extension not loaded by volk." << std::endl;
        throw std::runtime_error("VK_KHR_push_descriptor function not loaded");
    }
    std::cout << "vkCmdPushDescriptorSetKHR function pointer: " << (void*)vkCmdPushDescriptorSetKHR << std::endl;

    VrdxSorterCreateInfo createInfo{};
    createInfo.physicalDevice = m_vkState().m_physicalDevice;
    createInfo.device = m_vkState().m_device;
    createInfo.pipelineCache = VK_NULL_HANDLE;

    std::cout << "Creating radix sorter with:" << std::endl;
    std::cout << "  physicalDevice: " << createInfo.physicalDevice << std::endl;
    std::cout << "  device: " << createInfo.device << std::endl;

    vrdxCreateSorter(&createInfo, &m_radixSorter);

    if (m_radixSorter == VK_NULL_HANDLE) {
        std::cerr << "ERROR: vrdxCreateSorter returned NULL!" << std::endl;
        throw std::runtime_error("Failed to create radix sorter");
    }

    std::cout << "Radix sort initialized successfully, sorter handle: " << m_radixSorter << std::endl;
}

/**
 * @brief Cleanup depth sort pipeline
 */
void RendererGaussian::CleanupDepthSort() {
    auto& allocator = m_vkState().m_vmaAllocator;

    // Destroy storage buffers
    for (size_t i = 0; i < m_radixStorageBuffers.size(); ++i) {
        if (m_radixStorageBuffers[i] != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, m_radixStorageBuffers[i], m_radixStorageAllocations[i]);
        }
    }
    m_radixStorageBuffers.clear();
    m_radixStorageAllocations.clear();

    // Destroy radix sorter
    if (m_radixSorter != VK_NULL_HANDLE) {
        vrdxDestroySorter(m_radixSorter);
        m_radixSorter = VK_NULL_HANDLE;
    }

    std::cout << "Radix sort cleaned up" << std::endl;
}

/**
 * @brief Create command pools and command buffers (one per frame-in-flight)
 */
void RendererGaussian::CreateCommandBuffers() {
    auto& device = m_vkState().m_device;
    uint32_t frameCount = static_cast<uint32_t>(m_vkState().m_swapChain.m_swapChainImages.size());
    uint32_t queueFamilyIndex = m_vkState().m_queueFamilies.graphicsFamily.value();

    m_commandPools.resize(frameCount);
    m_commandBuffers.resize(frameCount);

    for (uint32_t i = 0; i < frameCount; i++) {
        // Create command pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndex;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        vkCreateCommandPool(device, &poolInfo, nullptr, &m_commandPools[i]);

        // Allocate command buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPools[i];
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        vkAllocateCommandBuffers(device, &allocInfo, &m_commandBuffers[i]);
    }

    std::cout << "Command buffers created (" << frameCount << " frames)" << std::endl;
}

/**
 * @brief Destroy command pools and command buffers
 */
void RendererGaussian::DestroyCommandBuffers() {
    auto& device = m_vkState().m_device;

    // Destroy command pools
    for (auto pool : m_commandPools) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, pool, nullptr);
        }
    }

    m_commandPools.clear();
    m_commandBuffers.clear();
}

/**
 * @brief Create descriptor pool for gaussian rendering
 */
void RendererGaussian::CreateDescriptorPool() {
    auto& device = m_vkState().m_device;
    uint32_t frameCount = static_cast<uint32_t>(m_vkState().m_swapChain.m_swapChainImages.size());

    // Pool sizes for descriptor sets (see CreateDescriptorSetLayouts for layout definitions)
    // Set 0: 1 uniform (Camera)
    // Set 1: 1 uniform (Info) + 4 storage (Position, Cov3d, Opacity, Sh)
    // Set 2: 6 storage (DrawIndirect, Instances, VisibleCount, Key, Index, InverseMap)
    // Set 3: 1 storage (PLY)
    // Total per object: 2 uniforms + 11 storage buffers
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frameCount * 10 },   // Camera (per-frame) + Info (per-object)
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, frameCount * 50 }    // All storage buffers (generous for multiple objects)
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = frameCount * 20;  // frameCount camera + 1 irradiance + 3 per object, sized for multiple objects

    vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool);
}

/**
 * @brief Allocate descriptor sets
 */
void RendererGaussian::AllocateDescriptorSets() {
    auto& device = m_vkState().m_device;
    uint32_t frameCount = static_cast<uint32_t>(m_vkState().m_swapChain.m_swapChainImages.size());

    m_cameraDescriptorSets.resize(frameCount);

    // Allocate camera descriptor sets (one per frame)
    for (uint32_t i = 0; i < frameCount; i++) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_cameraDescriptorLayout;

        vkAllocateDescriptorSets(device, &allocInfo, &m_cameraDescriptorSets[i]);
    }

    // Allocate irradiance descriptor set (single global set for env+irradiance cubemaps)
    VkDescriptorSetAllocateInfo irradianceAllocInfo{};
    irradianceAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    irradianceAllocInfo.descriptorPool = m_descriptorPool;
    irradianceAllocInfo.descriptorSetCount = 1;
    irradianceAllocInfo.pSetLayouts = &m_irradianceDescriptorLayout;
    vkAllocateDescriptorSets(device, &irradianceAllocInfo, &m_irradianceDescriptorSet);

    // Note: Gaussian data and output descriptor sets are allocated per object in OnObjectCreate()
}

/**
 * @brief Update descriptor sets with buffer bindings
 */
void RendererGaussian::UpdateDescriptorSets() {
    auto& device = m_vkState().m_device;
    uint32_t frameCount = static_cast<uint32_t>(m_vkState().m_swapChain.m_swapChainImages.size());

    // Update camera descriptor sets
    for (uint32_t i = 0; i < frameCount; i++) {
        VkDescriptorBufferInfo cameraBufferInfo{};
        cameraBufferInfo.buffer = m_cameraBuffers[i];
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet cameraWrite{};
        cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        cameraWrite.dstSet = m_cameraDescriptorSets[i];
        cameraWrite.dstBinding = 0;
        cameraWrite.dstArrayElement = 0;
        cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        cameraWrite.descriptorCount = 1;
        cameraWrite.pBufferInfo = &cameraBufferInfo;

        vkUpdateDescriptorSets(device, 1, &cameraWrite, 0, nullptr);
    }

    // Update irradiance cubemap descriptor set (for IBL convolution)
    if (m_irradianceDescriptorSet != VK_NULL_HANDLE) {
        // Binding 0: Environment cubemap (sampler)
        VkDescriptorImageInfo envDescriptorInfo = {};
        envDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        envDescriptorInfo.imageView = m_envCubemapView;
        envDescriptorInfo.sampler = m_envCubemapSampler;

        // Binding 1: Irradiance cubemap (storage image)
        VkDescriptorImageInfo irrDescriptorInfo = {};
        irrDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        irrDescriptorInfo.imageView = m_irradianceView;
        irrDescriptorInfo.sampler = VK_NULL_HANDLE;

        VkWriteDescriptorSet writes[2] = {};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_irradianceDescriptorSet;
        writes[0].dstBinding = 0;
        writes[0].dstArrayElement = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].descriptorCount = 1;
        writes[0].pImageInfo = &envDescriptorInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_irradianceDescriptorSet;
        writes[1].dstBinding = 1;
        writes[1].dstArrayElement = 0;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo = &irrDescriptorInfo;

        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
    }

    // Note: Gaussian data and output descriptor sets are updated per object in OnObjectCreate()
}

/**
 * @brief Destroy descriptors
 */
void RendererGaussian::DestroyDescriptors() {
    auto& device = m_vkState().m_device;

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    m_cameraDescriptorSets.clear();
    m_gaussianDataDescriptorSets.clear();
    m_outputDescriptorSets.clear();
    m_plyDescriptorSets.clear();
}

/**
 * @brief Allocate descriptor sets for a gaussian object
 * @param objectIndex Index into m_gaussianObjects vector
 */
void RendererGaussian::AllocateGaussianDescriptorSets(size_t objectIndex) {
    auto& device = m_vkState().m_device;
    auto& obj = m_gaussianObjects[objectIndex];

    // Allocate Set 1: Gaussian data descriptor set
    VkDescriptorSet gaussianDataSet;
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_gaussianDataDescriptorLayout;
    vkAllocateDescriptorSets(device, &allocInfo, &gaussianDataSet);
    m_gaussianDataDescriptorSets.push_back(gaussianDataSet);

    // Allocate Set 2: Output descriptor set
    VkDescriptorSet outputSet;
    VkDescriptorSetAllocateInfo outputAllocInfo{};
    outputAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    outputAllocInfo.descriptorPool = m_descriptorPool;
    outputAllocInfo.descriptorSetCount = 1;
    outputAllocInfo.pSetLayouts = &m_outputDescriptorLayout;
    vkAllocateDescriptorSets(device, &outputAllocInfo, &outputSet);
    m_outputDescriptorSets.push_back(outputSet);

    // Allocate Set 3: PLY raw data descriptor set
    VkDescriptorSet plySet;
    allocInfo.pSetLayouts = &m_plyDescriptorLayout;
    vkAllocateDescriptorSets(device, &allocInfo, &plySet);
    m_plyDescriptorSets.push_back(plySet);

    // Update Set 1: Gaussian data buffers
    std::vector<VkWriteDescriptorSet> writes;

    // Binding 0: Info uniform (point_count)
    VkDescriptorBufferInfo infoBufferInfo{};
    infoBufferInfo.buffer = obj.infoBuffer;
    infoBufferInfo.offset = 0;
    infoBufferInfo.range = sizeof(uint32_t);

    VkWriteDescriptorSet infoWrite{};
    infoWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    infoWrite.dstSet = gaussianDataSet;
    infoWrite.dstBinding = 0;
    infoWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    infoWrite.descriptorCount = 1;
    infoWrite.pBufferInfo = &infoBufferInfo;
    writes.push_back(infoWrite);

    // Binding 1: Position buffer
    VkDescriptorBufferInfo positionInfo{};
    positionInfo.buffer = obj.positionBuffer;
    positionInfo.offset = 0;
    positionInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet positionWrite{};
    positionWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    positionWrite.dstSet = gaussianDataSet;
    positionWrite.dstBinding = 1;
    positionWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    positionWrite.descriptorCount = 1;
    positionWrite.pBufferInfo = &positionInfo;
    writes.push_back(positionWrite);

    // Binding 2: Cov3d buffer
    VkDescriptorBufferInfo cov3dInfo{};
    cov3dInfo.buffer = obj.cov3dBuffer;
    cov3dInfo.offset = 0;
    cov3dInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet cov3dWrite{};
    cov3dWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cov3dWrite.dstSet = gaussianDataSet;
    cov3dWrite.dstBinding = 2;
    cov3dWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    cov3dWrite.descriptorCount = 1;
    cov3dWrite.pBufferInfo = &cov3dInfo;
    writes.push_back(cov3dWrite);

    // Binding 3: Opacity buffer
    VkDescriptorBufferInfo opacityInfo{};
    opacityInfo.buffer = obj.opacityBuffer;
    opacityInfo.offset = 0;
    opacityInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet opacityWrite{};
    opacityWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    opacityWrite.dstSet = gaussianDataSet;
    opacityWrite.dstBinding = 3;
    opacityWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    opacityWrite.descriptorCount = 1;
    opacityWrite.pBufferInfo = &opacityInfo;
    writes.push_back(opacityWrite);

    // Binding 4: SH buffer
    VkDescriptorBufferInfo shInfo{};
    shInfo.buffer = obj.shBuffer;
    shInfo.offset = 0;
    shInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet shWrite{};
    shWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    shWrite.dstSet = gaussianDataSet;
    shWrite.dstBinding = 4;
    shWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    shWrite.descriptorCount = 1;
    shWrite.pBufferInfo = &shInfo;
    writes.push_back(shWrite);

    // Update Set 2: Output buffers
    // Binding 0: DrawIndirect
    VkDescriptorBufferInfo drawIndirectInfo{};
    drawIndirectInfo.buffer = obj.drawIndirectBuffer;
    drawIndirectInfo.offset = 0;
    drawIndirectInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet drawIndirectWrite{};
    drawIndirectWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawIndirectWrite.dstSet = outputSet;
    drawIndirectWrite.dstBinding = 0;
    drawIndirectWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    drawIndirectWrite.descriptorCount = 1;
    drawIndirectWrite.pBufferInfo = &drawIndirectInfo;
    writes.push_back(drawIndirectWrite);

    // Binding 1: Instances
    VkDescriptorBufferInfo instancesInfo{};
    instancesInfo.buffer = obj.instancesBuffer;
    instancesInfo.offset = 0;
    instancesInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet instancesWrite{};
    instancesWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    instancesWrite.dstSet = outputSet;
    instancesWrite.dstBinding = 1;
    instancesWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    instancesWrite.descriptorCount = 1;
    instancesWrite.pBufferInfo = &instancesInfo;
    writes.push_back(instancesWrite);

    // Binding 2: Visible point count
    VkDescriptorBufferInfo visibleCountInfo{};
    visibleCountInfo.buffer = obj.visibleCountBuffer;
    visibleCountInfo.offset = 0;
    visibleCountInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet visibleCountWrite{};
    visibleCountWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    visibleCountWrite.dstSet = outputSet;
    visibleCountWrite.dstBinding = 2;
    visibleCountWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    visibleCountWrite.descriptorCount = 1;
    visibleCountWrite.pBufferInfo = &visibleCountInfo;
    writes.push_back(visibleCountWrite);

    // Binding 3: Key buffer
    VkDescriptorBufferInfo keyBufferInfo{};
    keyBufferInfo.buffer = obj.keyBuffer;
    keyBufferInfo.offset = 0;
    keyBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet keyWrite{};
    keyWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    keyWrite.dstSet = outputSet;
    keyWrite.dstBinding = 3;
    keyWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    keyWrite.descriptorCount = 1;
    keyWrite.pBufferInfo = &keyBufferInfo;
    writes.push_back(keyWrite);

    // Binding 4: Index buffer
    VkDescriptorBufferInfo indexInfo{};
    indexInfo.buffer = obj.indexBuffer;
    indexInfo.offset = 0;
    indexInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet indexWrite{};
    indexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    indexWrite.dstSet = outputSet;
    indexWrite.dstBinding = 4;
    indexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexWrite.descriptorCount = 1;
    indexWrite.pBufferInfo = &indexInfo;
    writes.push_back(indexWrite);

    // Binding 5: InverseMap
    VkDescriptorBufferInfo inverseMapInfo{};
    inverseMapInfo.buffer = obj.inverseMapBuffer;
    inverseMapInfo.offset = 0;
    inverseMapInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet inverseMapWrite{};
    inverseMapWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    inverseMapWrite.dstSet = outputSet;
    inverseMapWrite.dstBinding = 5;
    inverseMapWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    inverseMapWrite.descriptorCount = 1;
    inverseMapWrite.pBufferInfo = &inverseMapInfo;
    writes.push_back(inverseMapWrite);

    // Update Set 3: PLY raw data buffer
    // Binding 0: PLY buffer (offsets + raw data)
    VkDescriptorBufferInfo plyBufferInfo{};
    plyBufferInfo.buffer = obj.plyBuffer;
    plyBufferInfo.offset = 0;
    plyBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet plyWrite{};
    plyWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    plyWrite.dstSet = plySet;
    plyWrite.dstBinding = 0;
    plyWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    plyWrite.descriptorCount = 1;
    plyWrite.pBufferInfo = &plyBufferInfo;
    writes.push_back(plyWrite);

    // Execute all descriptor writes
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    // Allocate radix sort storage buffer
    VrdxSorterStorageRequirements requirements{};
    vrdxGetSorterKeyValueStorageRequirements(m_radixSorter, obj.pointCount, &requirements);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = requirements.size;
    bufferInfo.usage = requirements.usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo storageAllocInfo{};
    storageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkBuffer storageBuffer;
    VmaAllocation storageAllocation;
    vmaCreateBuffer(m_vkState().m_vmaAllocator, &bufferInfo, &storageAllocInfo, &storageBuffer, &storageAllocation, nullptr);

    m_radixStorageBuffers.push_back(storageBuffer);
    m_radixStorageAllocations.push_back(storageAllocation);

    std::cout << "Allocated radix sort storage: " << requirements.size << " bytes for " << obj.pointCount << " points" << std::endl;
    std::cout << "Allocated descriptor sets for gaussian object " << objectIndex << std::endl;

    // ============================================================================
    // PARSE PLY: Run during initialization to parse PLY data
    // Reference: third_party/vkgs/src/vkgs/engine/engine.cc:1137-1162
    // In VKGS, parse_ply runs on-demand per frame (vkgs:1138)
    // ============================================================================
    {
        auto& commandPool = m_commandPools[0];  // Use first frame's command pool for initialization

        VkCommandBuffer cmdBuffer;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);

        // vkgs:1141 - Bind parse_ply pipeline
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_parsePlyPipeline);

        // vkgs:1143-1149 - Bind descriptor sets (Set 1: gaussian, Set 3: ply)
        // We bind all three sets for consistency with our architecture
        VkDescriptorSet parsePlyDescriptorSets[] = {
            m_gaussianDataDescriptorSets[objectIndex],  // Set 1: vkgs:1143-1145
            m_outputDescriptorSets[objectIndex],        // Set 2: (our addition)
            m_plyDescriptorSets[objectIndex]            // Set 3: vkgs:1147-1149
        };
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_computePipelineLayout, 1, 3,
                                parsePlyDescriptorSets, 0, nullptr);

        // vkgs:1151-1152 - Dispatch parse_ply shader with workgroup calculation
        uint32_t parsePlyWorkgroups = (obj.pointCount + 255) / 256;
        vkCmdDispatch(cmdBuffer, parsePlyWorkgroups, 1, 1);

        // vkgs:1154-1158 - Barrier: ensure parse_ply writes complete before returning
        VkMemoryBarrier parseBarrier{};
        parseBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        parseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        parseBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmdBuffer,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0, 1, &parseBarrier, 0, nullptr, 0, nullptr);

        vkEndCommandBuffer(cmdBuffer);

        // Submit and wait
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        auto& graphicsQueue = m_vkState().m_graphicsQueue;
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &cmdBuffer);
    }
}

}  // namespace vve
