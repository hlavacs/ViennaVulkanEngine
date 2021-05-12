/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/


#include "VEInclude.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

namespace ve {

    VERendererRayTracingKHR::VERendererRayTracingKHR() : VERenderer() {
    }


    void VERendererRayTracingKHR::initRenderer() {
        VERenderer::initRenderer();

        const std::vector<const char *> requiredDeviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
            // Ray tracing related extensions
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            // Required by VK_KHR_acceleration_structure
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            // Required for VK_KHR_ray_tracing_pipeline
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            // Required by VK_KHR_spirv_1_4
            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
        };

        const std::vector<const char *> requiredValidationLayers = {
            "VK_LAYER_LUNARG_standard_validation"
        };

        vh::vhDevPickPhysicalDevice(getEnginePointer()->getInstance(), m_surface, requiredDeviceExtensions,
            &m_physicalDevice, &m_deviceFeatures, &m_deviceLimits);

        VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddresFeatures{};
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures{};
        VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures{};
        enabledBufferDeviceAddresFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        enabledBufferDeviceAddresFeatures.bufferDeviceAddress = VK_TRUE;

        enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
        enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddresFeatures;

        enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
        enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;

        if (vh::vhDevCreateLogicalDevice(getEnginePointer()->getInstance(), m_physicalDevice, m_surface, requiredDeviceExtensions, requiredValidationLayers, &enabledAccelerationStructureFeatures,
            &m_device, &m_graphicsQueue, &m_presentQueue) != VK_SUCCESS) {
            assert(false);
            exit(1);
        }

        vh::vhMemCreateVMAAllocator(getEnginePointer()->getInstance(), m_physicalDevice, m_device, m_vmaAllocator);

        vh::vhSwapCreateSwapChain(m_physicalDevice, m_surface, m_device, getWindowPointer()->getExtent(),
            &m_swapChain, m_swapChainImages, m_swapChainImageViews,
            &m_swapChainImageFormat, &m_swapChainExtent);

        //------------------------------------------------------------------------------------------------------------
        //create a command pools and the command buffers

        vh::vhCmdCreateCommandPool(m_physicalDevice, m_device, m_surface, &m_commandPool);	//command pool for the main thread

        m_commandPools.resize(getEnginePointer()->getThreadPool()->threadCount());			//each thread in the thread pool gets its own command pool
        for (uint32_t i = 0; i < m_commandPools.size(); i++) {
            vh::vhCmdCreateCommandPool(m_physicalDevice, m_device, m_surface, &m_commandPools[i]);
        }

        m_commandBuffers.resize(m_swapChainImages.size());
        for (uint32_t i = 0; i < m_swapChainImages.size(); i++) m_commandBuffers[i] = VK_NULL_HANDLE;	//will be created later

        m_secondaryBuffers.resize(m_swapChainImages.size());
        for (uint32_t i = 0; i < m_swapChainImages.size(); i++) m_secondaryBuffers[i] = {};	//will be created later

        m_secondaryBuffersFutures.resize(m_swapChainImages.size());

        //------------------------------------------------------------------------------------------------------------
        //create resources for light pass

        m_depthMap = new VETexture("DepthMap");
        m_depthMap->m_format = vh::vhDevFindDepthFormat(m_physicalDevice);
        m_depthMap->m_extent = m_swapChainExtent;

        vh::vhRenderCreateRenderPass(m_device, m_swapChainImageFormat, m_depthMap->m_format, VK_ATTACHMENT_LOAD_OP_CLEAR, &m_renderPassClear);
        vh::vhRenderCreateRenderPass(m_device, m_swapChainImageFormat, m_depthMap->m_format, VK_ATTACHMENT_LOAD_OP_LOAD, &m_renderPassLoad);

        //depth map for light pass
        vh::vhBufCreateDepthResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
            m_swapChainExtent, m_depthMap->m_format,
            &m_depthMap->m_image, &m_depthMap->m_deviceAllocation,
            &m_depthMap->m_imageInfo.imageView);

        //frame buffers for light pass
        std::vector<VkImageView> depthMaps;
        for (uint32_t i = 0; i < m_swapChainImageViews.size(); i++) depthMaps.push_back(m_depthMap->m_imageInfo.imageView);
        vh::vhBufCreateFramebuffers(m_device, m_swapChainImageViews, depthMaps, m_renderPassClear, m_swapChainExtent, m_swapChainFramebuffers);

        uint32_t maxobjects = 200;
        uint32_t storageobjects = m_swapChainImageViews.size();
        VECHECKRESULT(vh::vhRenderCreateDescriptorPool(m_device,
            {
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
              VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
            },
            { maxobjects, 1, storageobjects, maxobjects, maxobjects, maxobjects },
            &m_descriptorPool));

        VECHECKRESULT(vh::vhRenderCreateDescriptorSetLayout(m_device,
            { 1 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC },
            { VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV , },
            &m_descriptorSetLayoutPerObject));

        //------------------------------------------------------------------------------------------------------------

        for (uint32_t i = 0; i < m_swapChainImages.size(); i++) {
            vh::vhBufTransitionImageLayout(m_device, m_graphicsQueue, m_commandPool,				//transition the image layout to 
                m_swapChainImages[i], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,	//VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        }

        createSyncObjects();


        // Query the values of shaderHeaderSize and maxRecursionDepth in current implementation
        m_raytracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
        m_raytracingProperties.pNext = nullptr;
        VkPhysicalDeviceProperties2 props;
        props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props.pNext = &m_raytracingProperties;
        props.properties = {};
        vkGetPhysicalDeviceProperties2(m_physicalDevice, &props);

        m_raytracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        VkPhysicalDeviceFeatures2 deviceFeatures2{};
        deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures2.pNext = &m_raytracingFeatures;
        vkGetPhysicalDeviceFeatures2(m_physicalDevice, &deviceFeatures2);

        createSubrenderers();
    }

    /**
    * \brief Create and register all known subrenderers for this VERenderer
    */
    void VERendererRayTracingKHR::createSubrenderers() {
        addSubrenderer(new VESubrenderRayTracingKHR_DN(*this));
        //addSubrenderer(new VESubrender_Nuklear(*this));
    }

    void VERendererRayTracingKHR::addSubrenderer(VESubrender *pSub) {
        pSub->initSubrenderer();
        if (pSub->getClass() == VE_SUBRENDERER_CLASS_OVERLAY) {
            m_subrenderOverlay = pSub;
            return;
        }
        if (pSub->getClass() == VE_SUBRENDERER_CLASS_RT)
        {
            m_subrenderRT = (VESubrenderRayTracingKHR_DN *)pSub;
            return;
        }
    }

    /**
    * \brief Destroy the swapchain because window resize or close down
    */
    void VERendererRayTracingKHR::cleanupSwapChain()
    {
        delete m_depthMap;

        for (auto framebuffer : m_swapChainFramebuffers)
        {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        vkDestroyRenderPass(m_device, m_renderPassClear, nullptr);
        vkDestroyRenderPass(m_device, m_renderPassLoad, nullptr);

        for (auto imageView : m_swapChainImageViews)
        {
            vkDestroyImageView(m_device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
    }

    /**
    * \brief Close the renderer, destroy all local resources
    */
    void VERendererRayTracingKHR::closeRenderer()
    {
        destroySubrenderers();

        destroyAccelerationStructure(m_topLevelAS);
        for (auto &as : m_bottomLevelAS)
            destroyAccelerationStructure(as);

        vmaDestroyBuffer(m_vmaAllocator, m_instancesBuffer, m_instancesBufferAllocation);

        deleteCmdBuffers();

        cleanupSwapChain();

        //destroy per frame resources
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutPerObject, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        for (auto pool : m_commandPools)
            vkDestroyCommandPool(m_device, pool, nullptr);

        vmaDestroyAllocator(m_vmaAllocator);

        vkDestroyDevice(m_device, nullptr);
    }

    /**
    * \brief recreate the swapchain because the window size has changed
    */
    void VERendererRayTracingKHR::recreateSwapchain()
    {

        vkDeviceWaitIdle(m_device);

        cleanupSwapChain();

        vh::vhSwapCreateSwapChain(m_physicalDevice, m_surface, m_device, getWindowPointer()->getExtent(),
            &m_swapChain, m_swapChainImages, m_swapChainImageViews,
            &m_swapChainImageFormat, &m_swapChainExtent);

        m_depthMap = new VETexture("DepthMap");
        m_depthMap->m_format = vh::vhDevFindDepthFormat(m_physicalDevice);
        m_depthMap->m_extent = m_swapChainExtent;

        vh::vhRenderCreateRenderPass(m_device, m_swapChainImageFormat, m_depthMap->m_format, VK_ATTACHMENT_LOAD_OP_CLEAR, &m_renderPassClear);
        vh::vhRenderCreateRenderPass(m_device, m_swapChainImageFormat, m_depthMap->m_format, VK_ATTACHMENT_LOAD_OP_LOAD, &m_renderPassLoad);

        vh::vhBufCreateDepthResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, m_swapChainExtent,
            m_depthMap->m_format, &m_depthMap->m_image, &m_depthMap->m_deviceAllocation, &m_depthMap->m_imageInfo.imageView);

        std::vector<VkImageView> depthMaps;
        for (uint32_t i = 0; i < m_swapChainImageViews.size(); i++) depthMaps.push_back(m_depthMap->m_imageInfo.imageView);
        vh::vhBufCreateFramebuffers(m_device, m_swapChainImageViews, depthMaps, m_renderPassClear, m_swapChainExtent, m_swapChainFramebuffers);

        for (auto pSub : m_subrenderers) pSub->recreateResources();
        m_subrenderRT->recreateResources();
        deleteCmdBuffers();
    }

    /**
    * \brief Create the semaphores and fences for syncing command buffers and swapchain
    */
    void VERendererRayTracingKHR::createSyncObjects() {
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT); //for wait for the next swap chain image
        m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT); //for wait for render finished
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);		   //for wait for at least one image in the swap chain

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        m_overlaySemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
                assert(false);
                exit(1);
            }
        }
    }

    /**
    * \brief Delete all command buffers and set them to VK_NULL_HANDLE, so next time they have to be
    * created and recorded again
    */
    void VERendererRayTracingKHR::deleteCmdBuffers()
    {
        for (uint32_t i = 0; i < m_commandBuffers.size(); i++)
        {
            if (m_commandBuffers[i] != VK_NULL_HANDLE)
            {
                vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_commandBuffers[i]);
                m_commandBuffers[i] = VK_NULL_HANDLE;
            }
        }
    }

    //--------------------------------------------------------------------------------------------------
    //
    // Create a bottom-level acceleration structure based on a list of vertex
    // buffers in GPU memory along with their vertex count. The build is then done
    // in 3 steps: gathering the geometry, computing the sizes of the required
    // buffers, and building the actual acceleration structure #VKRay
    void VERendererRayTracingKHR::createBottomLevelAS(VkCommandBuffer commandBuffer, VEEntity *entity)
    {
        // Transform buffer
        VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
        VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
        VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};

        if (entity->m_pMesh->m_vertexBuffer != VK_NULL_HANDLE)
        {
            vertexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(entity->m_pMesh->m_vertexBuffer);
        }
        if (entity->m_pMesh->m_indexBuffer != VK_NULL_HANDLE)
        {
            indexBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(entity->m_pMesh->m_indexBuffer);
        }
        
        transformBufferDeviceAddress.deviceAddress = getBufferDeviceAddress(entity->m_memoryHandle.pMemBlock->buffers[0]);

        // Build
        VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        accelerationStructureGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        accelerationStructureGeometry.geometry.triangles.vertexData = vertexBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.maxVertex = entity->m_pMesh->m_vertexCount;
        accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(vh::vhVertex);
        accelerationStructureGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        accelerationStructureGeometry.geometry.triangles.indexData = indexBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.transformData.deviceAddress = 0;
        accelerationStructureGeometry.geometry.triangles.transformData.hostAddress = nullptr;
        accelerationStructureGeometry.geometry.triangles.transformData = transformBufferDeviceAddress;

        // Get size info
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
        accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

        const uint32_t numTriangles = entity->m_pMesh->m_indexCount/3;
        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(m_device,
                                                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                &accelerationStructureBuildGeometryInfo,
                                                &numTriangles,
                                                &accelerationStructureBuildSizesInfo);
        AccelerationStructure bottomLevelAS;
        createAccelerationStructure(bottomLevelAS, accelerationStructureBuildSizesInfo);
        
        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
        accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = bottomLevelAS.buffer;
        accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        vkCreateAccelerationStructureKHR(m_device, &accelerationStructureCreateInfo, nullptr, &bottomLevelAS.handle);

        // Create a small scratch buffer used during build of the bottom level acceleration structure
        RayTracingScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

        VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
        accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationBuildGeometryInfo.dstAccelerationStructure = bottomLevelAS.handle;
        accelerationBuildGeometryInfo.geometryCount = 1;
        accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
        accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
        accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = entity->m_memoryHandle.entryIndex * sizeof(VEEntity::veUBOPerEntity_t);
        std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };
        
        vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationBuildGeometryInfo, accelerationBuildStructureRangeInfos.data());

        VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
        accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationDeviceAddressInfo.accelerationStructure = bottomLevelAS.handle;
        bottomLevelAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(m_device, &accelerationDeviceAddressInfo);

        deleteScratchBuffer(scratchBuffer);

        m_bottomLevelAS.push_back(bottomLevelAS);
    }

    //--------------------------------------------------------------------------------------------------
    // Create the main acceleration structure that holds all instances of the scene.
    // Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
    // the instances, computing the memory requirements for the AS, and building the
    // AS itself #VKRay
    void VERendererRayTracingKHR::createTopLevelAS(VkCommandBuffer commandBuffer)
    {

        std::vector<VkAccelerationStructureInstanceKHR> asInstances;
        asInstances.reserve(m_bottomLevelAS.size());
        for (int i = 0; i < m_bottomLevelAS.size(); i++)
        {
            VkTransformMatrixKHR transformMatrix = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f };

            VkAccelerationStructureInstanceKHR instance{};
            instance.transform = transformMatrix;
            instance.instanceCustomIndex = i;
            instance.mask = 0xFF;
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            instance.accelerationStructureReference = m_bottomLevelAS[i].deviceAddress;
            asInstances.push_back(instance);
        }

        VkDeviceSize bufferSize = sizeof(asInstances[0]) * asInstances.size();
        VkBuffer stagingBuffer;
        VmaAllocation stagingBufferAllocation;
        VECHECKRESULT(vh::vhBufCreateBuffer(m_vmaAllocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, &stagingBuffer, &stagingBufferAllocation));
        void *data;
        VECHECKRESULT(vmaMapMemory(m_vmaAllocator, stagingBufferAllocation, &data));
        memcpy(data, asInstances.data(), (size_t)bufferSize);
        vmaUnmapMemory(m_vmaAllocator, stagingBufferAllocation);
        VECHECKRESULT(vh::vhBufCreateBuffer(m_vmaAllocator, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY, &m_instancesBuffer, &m_instancesBufferAllocation));
        VECHECKRESULT(vh::vhBufCopyBuffer(m_device, m_graphicsQueue, m_commandPool, stagingBuffer, m_instancesBuffer, bufferSize));
        vmaDestroyBuffer(m_vmaAllocator, stagingBuffer, stagingBufferAllocation);

        VkDeviceOrHostAddressConstKHR instancesAddress{};
        instancesAddress.deviceAddress = getBufferDeviceAddress(m_instancesBuffer);
        VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
        accelerationStructureGeometry.geometry.instances.data = instancesAddress;

        // Get size info
        /*
        The pSrcAccelerationStructure, dstAccelerationStructure, and mode members of pBuildInfo are ignored.
        Any VkDeviceOrHostAddressKHR members of pBuildInfo are ignored by this command, except that the hostAddress member 
        of VkAccelerationStructureGeometryTrianglesDataKHR::transformData will be examined to check if it is NULL.*
        */
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
        accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;

        uint32_t primitive_count = m_bottomLevelAS.size();

        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            m_device,
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &primitive_count,
            &accelerationStructureBuildSizesInfo);

        createAccelerationStructure(m_topLevelAS, accelerationStructureBuildSizesInfo);

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
        accelerationStructureCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = m_topLevelAS.buffer;
        accelerationStructureCreateInfo.size = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        vkCreateAccelerationStructureKHR(m_device, &accelerationStructureCreateInfo, nullptr, &m_topLevelAS.handle);

        // Create a small scratch buffer used during build of the top level acceleration structure
        RayTracingScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

        VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{};
        accelerationBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        accelerationBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationBuildGeometryInfo.dstAccelerationStructure = m_topLevelAS.handle;
        accelerationBuildGeometryInfo.geometryCount = 1;
        accelerationBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
        accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
        accelerationStructureBuildRangeInfo.primitiveCount = primitive_count;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationBuildStructureRangeInfos = { &accelerationStructureBuildRangeInfo };

        vkCmdBuildAccelerationStructuresKHR(
            commandBuffer,
            1,
            &accelerationBuildGeometryInfo,
            accelerationBuildStructureRangeInfos.data());

        VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
        accelerationDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationDeviceAddressInfo.accelerationStructure = m_topLevelAS.handle;
        m_topLevelAS.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(m_device, &accelerationDeviceAddressInfo);

        deleteScratchBuffer(scratchBuffer);
    }

    /*
    Gets the device address from a buffer that's required for some of the buffers used for ray tracing
    */
    uint64_t  VERendererRayTracingKHR::getBufferDeviceAddress(VkBuffer buffer)
    {
        VkBufferDeviceAddressInfoKHR bufferDeviceAI{};
        bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferDeviceAI.buffer = buffer;
        return vkGetBufferDeviceAddressKHR(m_device, &bufferDeviceAI);
    }

    VERendererRayTracingKHR::secondaryCmdBuf_t VERendererRayTracingKHR::recordRenderpass(VkRenderPass *pRenderPass,
        std::vector<VESubrender *> subRenderers,
        VkFramebuffer *pFrameBuffer,
        uint32_t imageIndex, uint32_t numPass,
        VECamera *pCamera, VELight *pLight)
    {
        secondaryCmdBuf_t buf;
        buf.pool = getThreadCommandPool();

        vh::vhCmdCreateCommandBuffers(m_device, buf.pool,
            VK_COMMAND_BUFFER_LEVEL_SECONDARY,
            1, &buf.buffer);

        vh::vhCmdBeginCommandBuffer(m_device, *pRenderPass, 0, *pFrameBuffer, buf.buffer,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        // there is only one subrenderer in ray tracing. All object ressources must be loaded at once.
        m_subrenderRT->draw(buf.buffer, imageIndex, numPass, pCamera, pLight);

        vkEndCommandBuffer(buf.buffer);

        return buf;

    }

    /**
    * \brief Create a new command buffer and record the whole scene into it, then end it
    */
    void VERendererRayTracingKHR::recordCmdBuffers() {
        VECamera *pCamera;
        VECHECKPOINTER(pCamera = getSceneManagerPointer()->getCamera());

        pCamera->setExtent(getWindowPointer()->getExtent());

        for (uint32_t i = 0; i < m_secondaryBuffers[m_imageIndex].size(); i++) {
            vkFreeCommandBuffers(m_device, m_secondaryBuffers[m_imageIndex][i].pool,
                1, &(m_secondaryBuffers[m_imageIndex][i].buffer));
        }
        m_secondaryBuffers[m_imageIndex].clear();

        m_secondaryBuffersFutures[m_imageIndex].clear();

        ThreadPool *tp = getEnginePointer()->getThreadPool();

        //-----------------------------------------------------------------------------------------------------------------
        //go through all active lights in the scene


        auto lights = getSceneManagerPointer()->getLights();

        std::chrono::high_resolution_clock::time_point t_start, t_now;
        t_start = vh::vhTimeNow();
        for (uint32_t i = 0; i < lights.size(); i++) {

            VELight *pLight = lights[i];


            //-----------------------------------------------------------------------------------------
            //light pass

            t_now = vh::vhTimeNow();
            {
                auto future = tp->add(&VERendererRayTracingKHR::recordRenderpass, this, &(i == 0 ? m_renderPassClear : m_renderPassLoad), m_subrenderers,
                    &m_swapChainFramebuffers[m_imageIndex],
                    m_imageIndex, i, pCamera, pLight);

                m_secondaryBuffersFutures[m_imageIndex].push_back(std::move(future));
            }
            m_AvgCmdLightTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgCmdLightTime);
        }

        //------------------------------------------------------------------------------------------
        //wait for all threads to finish and copy secondary command buffers into the vector

        m_secondaryBuffers[m_imageIndex].resize(m_secondaryBuffersFutures[m_imageIndex].size());
        for (uint32_t i = 0; i < m_secondaryBuffersFutures[m_imageIndex].size(); i++) {
            m_secondaryBuffers[m_imageIndex][i] = m_secondaryBuffersFutures[m_imageIndex][i].get();
        }

        //-----------------------------------------------------------------------------------------
        //set clear values for light pass

        std::vector<VkClearValue> clearValuesLight = {};	//render target and depth buffer should be cleared only first time
        VkClearValue cv1, cv2;
        cv1.color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clearValuesLight.push_back(cv1);
        cv2.depthStencil = { 1.0f, 0 };
        clearValuesLight.push_back(cv2);


        //-----------------------------------------------------------------------------------------
        //create a new primary command buffer and record all secondary buffers into it

        vh::vhCmdCreateCommandBuffers(m_device, m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1, &m_commandBuffers[m_imageIndex]);

        vh::vhCmdBeginCommandBuffer(m_device, m_commandBuffers[m_imageIndex], (VkCommandBufferUsageFlagBits)0);

        uint32_t bufferIdx = 0;
        for (uint32_t i = 0; i < lights.size(); i++) {

            VELight *pLight = lights[i];

            //vh::vhRenderBeginRenderPass(m_commandBuffers[m_imageIndex], i == 0 ? m_renderPassClear : m_renderPassLoad, m_swapChainFramebuffers[m_imageIndex], clearValuesLight, m_swapChainExtent, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
            vkCmdExecuteCommands(m_commandBuffers[m_imageIndex], 1, &m_secondaryBuffers[m_imageIndex][bufferIdx++].buffer);
            //vkCmdEndRenderPass(m_commandBuffers[m_imageIndex]);

            clearValuesLight.clear();		//since we blend the images onto each other, do not clear them for passes 2 and further
        }
        m_AvgRecordTime = vh::vhAverage(vh::vhTimeDuration(t_start), m_AvgRecordTime);

        vkEndCommandBuffer(m_commandBuffers[m_imageIndex]);

        m_overlaySemaphores[m_currentFrame] = m_renderFinishedSemaphores[m_currentFrame];


        //remember the last recorded entities, for incremental recording
        m_subrenderRT->afterDrawFinished();
    }

    /**
    * \brief Draw the frame.
    *
    *- wait for draw completion using a fence of a previous cmd buffer
    *- acquire the next image from the swap chain
    *- if there is no command buffer yet, record one with the current scene
    *- submit it to the queue
    */
    void VERendererRayTracingKHR::drawFrame()
    {
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

        //acquire the next image
        VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, std::numeric_limits<uint64_t>::max(),
            m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapchain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            assert(false);
            exit(1);
        }


        vh::vhBufTransitionImageLayout(m_device, m_graphicsQueue, m_commandPool,				//transition the image layout to 
            getSwapChainImage(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,		//VK_IMAGE_LAYOUT_GENERAL
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL);

        if (m_commandBuffers[m_imageIndex] == VK_NULL_HANDLE)
        {
            recordCmdBuffers();
        }

        //submit the command buffers
        vh::vhCmdSubmitCommandBuffer(m_device, m_graphicsQueue, m_commandBuffers[m_imageIndex],
            m_imageAvailableSemaphores[m_currentFrame],
            m_renderFinishedSemaphores[m_currentFrame],
            m_inFlightFences[m_currentFrame]);
    }

    /**
    * \brief Prepare to creat an overlay, e.g. initialize the next frame
    */
    void VERendererRayTracingKHR::prepareOverlay()
    {
        if (m_subrenderOverlay == nullptr) return;
        m_subrenderOverlay->prepareDraw();
    }


    /**
    * \brief Draw the overlay into the current frame buffer
    */
    void VERendererRayTracingKHR::drawOverlay()
    {
        if (m_subrenderOverlay == nullptr) return;

        m_overlaySemaphores[m_currentFrame] = m_subrenderOverlay->draw(m_imageIndex, m_renderFinishedSemaphores[m_currentFrame]);
    }

    /**
    * \brief Present the new frame.
    */
    void VERendererRayTracingKHR::presentFrame()
    {

        vh::vhBufTransitionImageLayout(m_device, m_graphicsQueue, m_commandPool,				//transition the image layout to 
            getSwapChainImage(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,		//VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VkResult result = vh::vhRenderPresentResult(m_presentQueue, m_swapChain, m_imageIndex,	//present it to the swap chain
            m_overlaySemaphores[m_currentFrame]);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized)
        {
            m_framebufferResized = false;
            recreateSwapchain();
        }
        else if (result != VK_SUCCESS)
        {
            assert(false);
            exit(1);
        }

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;		//count up the current frame number
    }

    //--------------------------------------------------------------------------------------------------
    // Create the bottom-level and top-level acceleration structures
    // #VKRay
    void VERendererRayTracingKHR::createAccelerationStructures()
    {

        // Create a one-time command buffer in which the AS build commands will be
        // issued
        VkCommandBufferAllocateInfo commandBufferAllocateInfo;
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.pNext = nullptr;
        commandBufferAllocateInfo.commandPool = m_commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VECHECKRESULT(vkAllocateCommandBuffers(getDevice(), &commandBufferAllocateInfo, &commandBuffer));

        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // For each geometric object, we compute the corresponding bottom-level
        // acceleration structure (BLAS)
        m_bottomLevelAS.clear();
        for (size_t i = 0; i < m_subrenderRT->m_entities.size(); i++)
        {
            createBottomLevelAS(commandBuffer, m_subrenderRT->m_entities[i]);
        }

        // Create the top-level AS from the previously computed BLAS
        createTopLevelAS(commandBuffer);

        //submit the command buffers
        VECHECKRESULT(vh::vhCmdEndSingleTimeCommands(m_device, m_graphicsQueue, m_commandPool, commandBuffer,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE
        ));
     }

    /*
    Create a scratch buffer to hold temporary data for a ray tracing acceleration structure
    */
    VERendererRayTracingKHR::RayTracingScratchBuffer VERendererRayTracingKHR::createScratchBuffer(VkDeviceSize size)
    {
        RayTracingScratchBuffer scratchBuffer{};

        VECHECKRESULT(vh::vhBufCreateBuffer(m_vmaAllocator, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, &scratchBuffer.buffer, &scratchBuffer.allocation));

        vmaMapMemory(m_vmaAllocator, scratchBuffer.allocation, &scratchBuffer.mapped);
        //vmaBindBufferMemory(m_vmaAllocator, scratchBuffer.allocation, scratchBuffer.buffer);
        scratchBuffer.deviceAddress = getBufferDeviceAddress(scratchBuffer.buffer);

        return scratchBuffer;
    }

    void  VERendererRayTracingKHR::deleteScratchBuffer(RayTracingScratchBuffer &scratchBuffer)
    {
        vmaUnmapMemory(m_vmaAllocator, scratchBuffer.allocation);
        vmaDestroyBuffer(m_vmaAllocator, scratchBuffer.buffer, scratchBuffer.allocation);
    }

    void VERendererRayTracingKHR::createAccelerationStructure(AccelerationStructure &accelerationStructure, VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo)
    {
        VECHECKRESULT(vh::vhBufCreateBuffer(m_vmaAllocator, buildSizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY,
            &accelerationStructure.buffer, &accelerationStructure.allocation));
    }

    void VERendererRayTracingKHR::destroyAccelerationStructure(const AccelerationStructure &as)
    {
        vmaDestroyBuffer(m_vmaAllocator, as.buffer, as.allocation);
        vkDestroyAccelerationStructureKHR(m_device, as.handle, nullptr);
    }

    // this function will be called as the first step in engine run() method if m_ray_tracing engine flag is enabled
    // it creates Bottom and Top level acceleration structures and bind descriptor sets with surenderer ressources, such as
    // output image, acceleration structure, vertex and index buffers and entity UBOs
    void VERendererRayTracingKHR::initAccelerationStructures()
    {
        if (m_subrenderRT->m_entities.size())
        {
            createAccelerationStructures();
            m_subrenderRT->UpdateRTDescriptorSets();
        }
    }

    void VERendererRayTracingKHR::addEntityToSubrenderer(VEEntity *pEntity) {

        veSubrenderType type = VE_SUBRENDERER_TYPE_NONE;

        switch (pEntity->getEntityType()) {
        case VEEntity::VE_ENTITY_TYPE_NORMAL:
            if (pEntity->m_pMaterial->mapDiffuse != nullptr) {

                if (pEntity->m_pMaterial->mapNormal != nullptr) {
                    type = VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP;
                    break;
                }

                type = VE_SUBRENDERER_TYPE_DIFFUSEMAP;
                break;
            }
            type = VE_SUBRENDERER_TYPE_COLOR1;
            break;
        case VEEntity::VE_ENTITY_TYPE_SKYPLANE:
            type = VE_SUBRENDERER_TYPE_SKYPLANE;
            break;
        case VEEntity::VE_ENTITY_TYPE_TERRAIN_HEIGHTMAP:
            break;
        default: return;
        }

        if (type == VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP || type == VE_SUBRENDERER_TYPE_DIFFUSEMAP)
        {
            m_subrenderRT->addEntity(pEntity);
        }
    }
}