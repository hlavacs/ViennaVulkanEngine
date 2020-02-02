/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/


#include "VEInclude.h"

#include "VKHelpers.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

namespace ve {
	VERendererRT* g_pVERendererRTSingleton = nullptr;	///<Singleton pointer to the only VERendererRT instance

	VERendererRT::VERendererRT() : VERenderer() {
		g_pVERendererRTSingleton = this;
	}


	void VERendererRT::initRenderer() {
		VERenderer::initRenderer();

		const std::vector<const char*> requiredDeviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
			VK_NV_RAY_TRACING_EXTENSION_NAME
		};

		const std::vector<const char*> requiredValidationLayers = {
			"VK_LAYER_LUNARG_standard_validation"
		};

		vh::vhDevPickPhysicalDevice(getEnginePointer()->getInstance(), m_surface, requiredDeviceExtensions,
			&m_physicalDevice, &m_deviceFeatures, &m_deviceLimits);

		if (vh::vhDevCreateLogicalDevice(getEnginePointer()->getInstance(), m_physicalDevice, m_surface, requiredDeviceExtensions, requiredValidationLayers,
			&m_device, &m_graphicsQueue, &m_presentQueue) != VK_SUCCESS) {
			assert(false);
			exit(1);
		}

		vh::vhMemCreateVMAAllocator(m_physicalDevice, m_device, m_vmaAllocator);

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
        for(uint32_t i = 0; i < m_swapChainImages.size(); i++) m_secondaryBuffers[i] = {};	//will be created later

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

        uint32_t maxobjects = 50;
		VECHECKRESULT(vh::vhRenderCreateDescriptorPool(m_device,
			{
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
              VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV,
              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
            },
			{ maxobjects, 1, 1, maxobjects, maxobjects, maxobjects },
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
        m_raytracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
        m_raytracingProperties.pNext = nullptr;
        m_raytracingProperties.maxRecursionDepth = 0;
        m_raytracingProperties.shaderGroupHandleSize = 0;
        VkPhysicalDeviceProperties2 props;
        props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        props.pNext = &m_raytracingProperties;
        props.properties = {};
        vkGetPhysicalDeviceProperties2(m_physicalDevice, &props);

		createSubrenderers();
	}

	/**
	* \brief Create and register all known subrenderers for this VERenderer
	*/
	void VERendererRT::createSubrenderers() {
		addSubrenderer(new VESubrenderRT_DN());
        addSubrenderer(new VESubrenderFW_Nuklear());
	}

	void VERendererRT::addSubrenderer(VESubrender* pSub) {
		pSub->initSubrenderer();
		if (pSub->getClass() == VESubrender::VE_SUBRENDERER_CLASS_OVERLAY) {
			m_subrenderOverlay = pSub;
			return;
		}
        if(pSub->getClass() == VESubrender::VE_SUBRENDERER_CLASS_RT)
        {
            m_subrenderRT = (VESubrenderRT_DN *)pSub;
            return;
        }
	}

    /**
    * \brief Destroy the swapchain because window resize or close down
    */
    void VERendererRT::cleanupSwapChain()
    {
        delete m_depthMap;

        for(auto framebuffer : m_swapChainFramebuffers)
        {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
        }

        vkDestroyRenderPass(m_device, m_renderPassClear, nullptr);
        vkDestroyRenderPass(m_device, m_renderPassLoad, nullptr);

        for(auto imageView : m_swapChainImageViews)
        {
            vkDestroyImageView(m_device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
    }

    /**
    * \brief Close the renderer, destroy all local resources
    */
    void VERendererRT::closeRenderer()
    {
        destroySubrenderers();

        destroyAccelerationStructure(m_topLevelAS);
        for(auto &as : m_bottomLevelAS)
            destroyAccelerationStructure(as);

        deleteCmdBuffers();

        cleanupSwapChain();

        //destroy per frame resources
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutPerObject, nullptr);

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        for(auto pool : m_commandPools)
            vkDestroyCommandPool(m_device, pool, nullptr);

        vmaDestroyAllocator(m_vmaAllocator);

        vkDestroyDevice(m_device, nullptr);
    }

    /**
    * \brief recreate the swapchain because the window size has changed
    */
    void VERendererRT::recreateSwapchain()
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
        for(uint32_t i = 0; i < m_swapChainImageViews.size(); i++) depthMaps.push_back(m_depthMap->m_imageInfo.imageView);
        vh::vhBufCreateFramebuffers(m_device, m_swapChainImageViews, depthMaps, m_renderPassClear, m_swapChainExtent, m_swapChainFramebuffers);

        for(auto pSub : m_subrenderers) pSub->recreateResources();
        m_subrenderRT->recreateResources();
        deleteCmdBuffers();
    }

	/**
	* \brief Create the semaphores and fences for syncing command buffers and swapchain
	*/
	void VERendererRT::createSyncObjects() {
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
    void VERendererRT::deleteCmdBuffers()
    {
        for(uint32_t i = 0; i < m_commandBuffers.size(); i++)
        {
            if(m_commandBuffers[i] != VK_NULL_HANDLE)
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
	VERendererRT::AccelerationStructure VERendererRT::createBottomLevelAS(
		VkCommandBuffer               commandBuffer,
		std::vector<VEEntity *> Entities)
	{

		// Adding all vertex buffers and not transforming their position.
		for (const auto& entity : Entities)
		{
			if (entity->m_pMesh->m_indexBuffer == VK_NULL_HANDLE)
			{
                // Transform Buffer must have an 4x3 affine transformation matrix
				// No indices
                m_BottomLevelASGenerator.AddVertexBuffer(entity->m_pMesh->m_vertexBuffer, 0, entity->m_pMesh->m_vertexCount,
					sizeof(vh::vhVertex), VK_NULL_HANDLE, 0);
			}
			else
			{
                // Transform Buffer must have an 4x3 affine transformation matrix
				// Indexed geometry
                m_BottomLevelASGenerator.AddVertexBuffer(entity->m_pMesh->m_vertexBuffer, 0, entity->m_pMesh->m_vertexCount,
					sizeof(vh::vhVertex), entity->m_pMesh->m_indexBuffer, 0,
					entity->m_pMesh->m_indexCount, VK_NULL_HANDLE, 0);
			}
		}
		AccelerationStructure buffers;

		// Once the overall size of the geometry is known, we can create the handle
		// for the acceleration structure
		buffers.structure = m_BottomLevelASGenerator.CreateAccelerationStructure(m_device, VK_FALSE);

		// The AS build requires some scratch space to store temporary information.
		// The amount of scratch memory is dependent on the scene complexity.
		VkDeviceSize scratchSizeInBytes = 0;
		// The final AS also needs to be stored in addition to the existing vertex
		// buffers. It size is also dependent on the scene complexity.
		VkDeviceSize resultSizeInBytes = 0;
        m_BottomLevelASGenerator.ComputeASBufferSizes(m_device, buffers.structure, &scratchSizeInBytes,
			&resultSizeInBytes);
		
		// Once the sizes are obtained, the application is responsible for allocating
		// the necessary buffers. Since the entire generation will be done on the GPU,
		// we can directly allocate those in device local mem
		nv_helpers_vk::createBuffer(m_physicalDevice, m_device, scratchSizeInBytes,
			VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, &buffers.scratchBuffer,
			&buffers.scratchMem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		nv_helpers_vk::createBuffer(m_physicalDevice, m_device, resultSizeInBytes,
			VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, &buffers.resultBuffer,
			&buffers.resultMem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Build the acceleration structure. Note that this call integrates a barrier
		// on the generated AS, so that it can be used to compute a top-level AS right
		// after this method.
        m_BottomLevelASGenerator.Generate(m_device, commandBuffer, buffers.structure, buffers.scratchBuffer,
			0, buffers.resultBuffer, buffers.resultMem, false, VK_NULL_HANDLE);

		return buffers;
	}

	//--------------------------------------------------------------------------------------------------
	// Create the main acceleration structure that holds all instances of the scene.
	// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
	// the instances, computing the memory requirements for the AS, and building the
	// AS itself #VKRay
	void VERendererRT::createTopLevelAS(VkCommandBuffer commandBuffer, const std::vector<std::pair<VkAccelerationStructureNV, glm::mat4x4>> &instances,  // pair of bottom level AS and matrix of the instance
		VkBool32 updateOnly)
	{

		if (!updateOnly)
		{
			// Gather all the instances into the builder helper
			for (size_t i = 0; i < instances.size(); i++)
			{
				// For each instance we set its instance index to its index i in the instance vector, and set
				// its hit group index to 0. The hit group index defines which entry of the shader binding
				// table will contain the hit group to be executed when hitting this instance.
                m_TopLevelASGenerator.AddInstance(instances[i].first, instances[i].second,
					static_cast<uint32_t>(i), static_cast<uint32_t>(0));
			}

			// Once all instances have been added, we can create the handle for the TLAS
			m_topLevelAS.structure =
                m_TopLevelASGenerator.CreateAccelerationStructure(getDevice(), VK_TRUE);


			// As for the bottom-level AS, the building the AS requires some scratch
			// space to store temporary data in addition to the actual AS. In the case
			// of the top-level AS, the instance descriptors also need to be stored in
			// GPU memory. This call outputs the memory requirements for each (scratch,
			// results, instance descriptors) so that the application can allocate the
			// corresponding memory
			VkDeviceSize scratchSizeInBytes, resultSizeInBytes, instanceDescsSizeInBytes;
            m_TopLevelASGenerator.ComputeASBufferSizes(getDevice(), m_topLevelAS.structure,
				&scratchSizeInBytes, &resultSizeInBytes,
				&instanceDescsSizeInBytes);

			// Create the scratch and result buffers. Since the build is all done on
			// GPU, those can be allocated in device local memory
			nv_helpers_vk::createBuffer(getPhysicalDevice(), getDevice(), scratchSizeInBytes,
				VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, &m_topLevelAS.scratchBuffer,
				&m_topLevelAS.scratchMem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			nv_helpers_vk::createBuffer(getPhysicalDevice(), getDevice(), resultSizeInBytes,
				VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, &m_topLevelAS.resultBuffer,
				&m_topLevelAS.resultMem, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			// The buffer describing the instances: ID, shader binding information,
			// matrices ... Those will be copied into the buffer by the helper through
			// mapping, so the buffer has to be allocated in host visible memory.

			nv_helpers_vk::createBuffer(getPhysicalDevice(), getDevice(),
				instanceDescsSizeInBytes, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
				&m_topLevelAS.instancesBuffer, &m_topLevelAS.instancesMem,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}

		// After all the buffers are allocated, or if only an update is required, we
		// can build the acceleration structure. Note that in the case of the update
		// we also pass the existing AS as the 'previous' AS, so that it can be
		// refitted in place. Build the acceleration structure. Note that this call
		// integrates a barrier on the generated AS, so that it can be used to compute
		// a top-level AS right after this method.
        m_TopLevelASGenerator.Generate(getDevice(), commandBuffer, m_topLevelAS.structure,
			m_topLevelAS.scratchBuffer, 0, m_topLevelAS.resultBuffer,
			m_topLevelAS.resultMem, m_topLevelAS.instancesBuffer,
			m_topLevelAS.instancesMem, updateOnly,
			updateOnly ? m_topLevelAS.structure : VK_NULL_HANDLE);
	}

    VERendererRT::secondaryCmdBuf_t VERendererRT::recordRenderpass(VkRenderPass *pRenderPass,
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
            VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT);

        // there is only one subrenderer in ray tracing. All object ressources must be loaded at once.
        m_subrenderRT->draw(buf.buffer, imageIndex, numPass, pCamera, pLight);

        vkEndCommandBuffer(buf.buffer);

        return buf;

    }

    /**
    * \brief Create a new command buffer and record the whole scene into it, then end it
    */
    void VERendererRT::recordCmdBuffers() {
        VECamera* pCamera;
        VECHECKPOINTER(pCamera = getSceneManagerPointer()->getCamera());

        pCamera->setExtent(getWindowPointer()->getExtent());

        for(uint32_t i = 0; i < m_secondaryBuffers[m_imageIndex].size(); i++) {
            vkFreeCommandBuffers(m_device, m_secondaryBuffers[m_imageIndex][i].pool,
                1, &(m_secondaryBuffers[m_imageIndex][i].buffer));
        }
        m_secondaryBuffers[m_imageIndex].clear();

        m_secondaryBuffersFutures[m_imageIndex].clear();

        ThreadPool* tp = getEnginePointer()->getThreadPool();

        //-----------------------------------------------------------------------------------------------------------------
        //go through all active lights in the scene


        auto lights = getSceneManagerPointer()->getLights();

        std::chrono::high_resolution_clock::time_point t_start, t_now;
        t_start = vh::vhTimeNow();
        for(uint32_t i = 0; i < lights.size(); i++) {

            VELight* pLight = lights[i];


            //-----------------------------------------------------------------------------------------
            //light pass

            t_now = vh::vhTimeNow();
            {
                auto future = tp->add(&VERendererRT::recordRenderpass, this, &(i == 0 ? m_renderPassClear : m_renderPassLoad), m_subrenderers,
                    &m_swapChainFramebuffers[m_imageIndex],
                    m_imageIndex, i, pCamera, pLight);

                m_secondaryBuffersFutures[m_imageIndex].push_back(std::move(future));
            }
            m_AvgCmdLightTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgCmdLightTime);
        }

        //------------------------------------------------------------------------------------------
        //wait for all threads to finish and copy secondary command buffers into the vector

        m_secondaryBuffers[m_imageIndex].resize(m_secondaryBuffersFutures[m_imageIndex].size());
        for(uint32_t i = 0; i < m_secondaryBuffersFutures[m_imageIndex].size(); i++) {
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
        for(uint32_t i = 0; i < lights.size(); i++) {

            VELight* pLight = lights[i];

            vh::vhRenderBeginRenderPass(m_commandBuffers[m_imageIndex], i == 0 ? m_renderPassClear : m_renderPassLoad, m_swapChainFramebuffers[m_imageIndex], clearValuesLight, m_swapChainExtent, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
            vkCmdExecuteCommands(m_commandBuffers[m_imageIndex], 1, &m_secondaryBuffers[m_imageIndex][bufferIdx++].buffer);
            vkCmdEndRenderPass(m_commandBuffers[m_imageIndex]);

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
    void VERendererRT::drawFrame()
    {
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

        //acquire the next image
        VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, std::numeric_limits<uint64_t>::max(),
            m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);

        if(result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapchain();
            return;
        }
        else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            assert(false);
            exit(1);
        }

        if(m_commandBuffers[m_imageIndex] == VK_NULL_HANDLE)
        {
            recordCmdBuffers();
        }

        vh::vhBufTransitionImageLayout(m_device, m_graphicsQueue, m_commandPool,				//transition the image layout to 
            getSwapChainImage(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,		//VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        //submit the command buffers
        vh::vhCmdSubmitCommandBuffer(m_device, m_graphicsQueue, m_commandBuffers[m_imageIndex],
            m_imageAvailableSemaphores[m_currentFrame],
            m_renderFinishedSemaphores[m_currentFrame],
            m_inFlightFences[m_currentFrame]);
    }

    /**
    * \brief Prepare to creat an overlay, e.g. initialize the next frame
    */
    void VERendererRT::prepareOverlay()
    {
        if(m_subrenderOverlay == nullptr) return;
        m_subrenderOverlay->prepareDraw();
    }


    /**
    * \brief Draw the overlay into the current frame buffer
    */
    void VERendererRT::drawOverlay()
    {
        if(m_subrenderOverlay == nullptr) return;

        m_overlaySemaphores[m_currentFrame] = m_subrenderOverlay->draw(m_imageIndex, m_renderFinishedSemaphores[m_currentFrame]);
    }

    /**
    * \brief Present the new frame.
    */
    void VERendererRT::presentFrame()
    {

        vh::vhBufTransitionImageLayout(m_device, m_graphicsQueue, m_commandPool,				//transition the image layout to 
            getSwapChainImage(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,		//VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VkResult result = vh::vhRenderPresentResult(m_presentQueue, m_swapChain, m_imageIndex,	//present it to the swap chain
                                                    m_overlaySemaphores[m_currentFrame]);

        if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized)
        {
            m_framebufferResized = false;
            recreateSwapchain();
        }
        else if(result != VK_SUCCESS)
        {
            assert(false);
            exit(1);
        }

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;		//count up the current frame number
    }

	//--------------------------------------------------------------------------------------------------
	// Create the bottom-level and top-level acceleration structures
	// #VKRay
	void VERendererRT::createAccelerationStructures()
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
		VkResult        code =
			vkAllocateCommandBuffers(getDevice(), &commandBufferAllocateInfo, &commandBuffer);
		if (code != VK_SUCCESS)
		{
			throw std::logic_error("rt vkAllocateCommandBuffers failed");
		}

		VkCommandBufferBeginInfo beginInfo;
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.pNext = nullptr;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beginInfo.pInheritanceInfo = nullptr;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);


		// For each geometric object, we compute the corresponding bottom-level
		// acceleration structure (BLAS)
        m_bottomLevelAS.clear();
		m_bottomLevelAS.resize(m_subrenderRT->m_entities.size());

		std::vector<std::pair<VkAccelerationStructureNV, glm::mat4x4>> instances;

		for (size_t i = 0; i < m_subrenderRT->m_entities.size(); i++)
		{
			m_bottomLevelAS[i] = createBottomLevelAS(
				commandBuffer, { m_subrenderRT->m_entities[i]});
			instances.push_back({ m_bottomLevelAS[i].structure, m_subrenderRT->m_entities[i]->getWorldTransform() });
		}

		// Create the top-level AS from the previously computed BLAS
		createTopLevelAS(commandBuffer, instances, VK_FALSE);

		//submit the command buffers
		VkResult result = vh::vhCmdEndSingleTimeCommands(m_device, m_graphicsQueue, m_commandPool, commandBuffer,
			VK_NULL_HANDLE,
			VK_NULL_HANDLE,
			VK_NULL_HANDLE
		);
		if(result)
		{
			throw std::logic_error("rt createAccelerationStructures failed");
		}
	}

    void VERendererRT::destroyAccelerationStructure(const AccelerationStructure &as)
    {
        vkDestroyBuffer(m_device, as.scratchBuffer, nullptr);
        vkFreeMemory(m_device, as.scratchMem, nullptr);
        vkDestroyBuffer(m_device, as.resultBuffer, nullptr);
        vkFreeMemory(m_device, as.resultMem, nullptr);
        vkDestroyBuffer(m_device, as.instancesBuffer, nullptr);
        vkFreeMemory(m_device, as.instancesMem, nullptr);
        vkDestroyAccelerationStructureNV(m_device, as.structure, nullptr);
    }

    // this function will be called as the first step in engine run() method if m_ray_tracing engine flag is enabled
    // it creates Bottom and Top level acceleration structures and bind descriptor sets with surenderer ressources, such as
    // output image, acceleration structure, vertex and index buffers and entity UBOs
	void VERendererRT::initAccelerationStructures()
	{
        if(m_subrenderRT->m_entities.size())
        {
            createAccelerationStructures();
            m_subrenderRT->UpdateRTDescriptorSets();
        }
	}

	void VERendererRT::addEntityToSubrenderer(VEEntity* pEntity) {

		VESubrender::veSubrenderType type = VESubrender::VE_SUBRENDERER_TYPE_NONE;

		switch (pEntity->getEntityType()) {
		case VEEntity::VE_ENTITY_TYPE_NORMAL:
			if (pEntity->m_pMaterial->mapDiffuse != nullptr) {

				if (pEntity->m_pMaterial->mapNormal != nullptr) {
					type = VESubrender::VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP;
					break;
				}

				type = VESubrender::VE_SUBRENDERER_TYPE_DIFFUSEMAP;
				break;
			}
			type = VESubrender::VE_SUBRENDERER_TYPE_COLOR1;
			break;
		case VEEntity::VE_ENTITY_TYPE_SKYPLANE:
			type = VESubrender::VE_SUBRENDERER_TYPE_SKYPLANE;
			break;
		case VEEntity::VE_ENTITY_TYPE_TERRAIN_HEIGHTMAP:
			break;
		default: return;
		}

		if (type == VESubrender::VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP || type == VESubrender::VE_SUBRENDERER_TYPE_DIFFUSEMAP)
		{
			m_subrenderRT->addEntity(pEntity);
		}
	}
}