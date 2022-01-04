/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
*
*/

#include "VEInclude.h"

const int MAX_FRAMES_IN_FLIGHT = 2;

namespace ve
{
	VERendererRayTracingKHR::VERendererRayTracingKHR()
		: VERenderer(),
		m_topLevelAS{}
	{
	}

	void VERendererRayTracingKHR::initRenderer()
	{
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
			"VK_LAYER_LUNARG_standard_validation" };

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

		if (vh::vhDevPickPhysicalDevice(getEnginePointer()->getInstance(), m_surface, requiredDeviceExtensions,
			&m_physicalDevice, &m_deviceFeatures, &m_deviceLimits) != VK_SUCCESS ||
			vh::vhDevCreateLogicalDevice(getEnginePointer()->getInstance(), m_physicalDevice, m_surface,
				requiredDeviceExtensions, requiredValidationLayers,
				&enabledBufferDeviceAddresFeatures, &m_device, &m_graphicsQueue,
				&m_presentQueue) != VK_SUCCESS)
		{
			assert(false);
			exit(1);
		}

		vh::vhMemCreateVMAAllocator(getEnginePointer()->getInstance(), m_physicalDevice, m_device, m_vmaAllocator);

		vh::vhSwapCreateSwapChain(m_physicalDevice, m_surface, m_device, getWindowPointer()->getExtent(),
			&m_swapChain, m_swapChainImages, m_swapChainImageViews,
			&m_swapChainImageFormat, &m_swapChainExtent);

		//------------------------------------------------------------------------------------------------------------
		//create a command pools and the command buffers

		vh::vhCmdCreateCommandPool(m_physicalDevice, m_device, m_surface,
			&m_commandPool); //command pool for the main thread

		m_commandPools.resize(
			getEnginePointer()->getThreadPool()->threadCount()); //each thread in the thread pool gets its own command pool
		for (uint32_t i = 0; i < m_commandPools.size(); i++)
		{
			vh::vhCmdCreateCommandPool(m_physicalDevice, m_device, m_surface, &m_commandPools[i]);
		}

		m_commandBuffers.resize(m_swapChainImages.size());
		for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
			m_commandBuffers[i] = VK_NULL_HANDLE; //will be created later

		m_secondaryBuffers.resize(m_swapChainImages.size());
		for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
			m_secondaryBuffers[i] = {}; //will be created later

		m_secondaryBuffersFutures.resize(m_swapChainImages.size());

		//------------------------------------------------------------------------------------------------------------
		//create resources for light pass

		m_depthMap = new VETexture("DepthMap");
		m_depthMap->m_format = vh::vhDevFindDepthFormat(m_physicalDevice);
		m_depthMap->m_extent = m_swapChainExtent;

		vh::vhRenderCreateRenderPassRayTracing(m_device, m_swapChainImageFormat, m_depthMap->m_format, &m_renderPass);

		//depth map for light pass
		vh::vhBufCreateDepthResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
			m_swapChainExtent, m_depthMap->m_format,
			&m_depthMap->m_image, &m_depthMap->m_deviceAllocation,
			&m_depthMap->m_imageInfo.imageView);

		//frame buffers for light pass
		std::vector<VkImageView> depthMaps;
		for (uint32_t i = 0; i < m_swapChainImageViews.size(); i++)
			depthMaps.push_back(m_depthMap->m_imageInfo.imageView);
		vh::vhBufCreateFramebuffers(m_device, m_swapChainImageViews, depthMaps, m_renderPass, m_swapChainExtent,
			m_swapChainFramebuffers);

		uint32_t maxobjects = 200;
		uint32_t storageobjects = m_swapChainImageViews.size();
		VECHECKRESULT(vh::vhRenderCreateDescriptorPool(m_device,
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			 VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			 VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			 VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
			{ maxobjects, 1, storageobjects, maxobjects, maxobjects,
			 maxobjects },
			&m_descriptorPool));

		VECHECKRESULT(vh::vhRenderCreateDescriptorSetLayout(m_device,
			{ 1 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC },
														{
															VK_SHADER_STAGE_RAYGEN_BIT_KHR |
																VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
														},
			&m_descriptorSetLayoutPerObject));

		//------------------------------------------------------------------------------------------------------------

		for (uint32_t i = 0; i < m_swapChainImages.size(); i++)
		{
			vh::vhBufTransitionImageLayout(m_device, m_graphicsQueue,
				m_commandPool, //transition the image layout to
				m_swapChainImages[i], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1,
				1, //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
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
	void VERendererRayTracingKHR::createSubrenderers()
	{
		addSubrenderer(new VESubrenderRayTracingKHR_DN(*this));
		addSubrenderer(new VESubrender_Nuklear(*this));
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

		vkDestroyRenderPass(m_device, m_renderPass, nullptr);

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

		vh::vhDestroyAccelerationStructure(m_device, m_vmaAllocator, m_topLevelAS);

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

		vh::vhRenderCreateRenderPass(m_device, m_swapChainImageFormat, m_depthMap->m_format,
			VK_ATTACHMENT_LOAD_OP_CLEAR, &m_renderPass);

		vh::vhBufCreateDepthResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, m_swapChainExtent,
			m_depthMap->m_format, &m_depthMap->m_image, &m_depthMap->m_deviceAllocation,
			&m_depthMap->m_imageInfo.imageView);

		std::vector<VkImageView> depthMaps;
		for (uint32_t i = 0; i < m_swapChainImageViews.size(); i++)
			depthMaps.push_back(m_depthMap->m_imageInfo.imageView);
		vh::vhBufCreateFramebuffers(m_device, m_swapChainImageViews, depthMaps, m_renderPass, m_swapChainExtent,
			m_swapChainFramebuffers);

		for (auto pSub : m_subrenderers)
			pSub->recreateResources();
		m_subrenderRT->recreateResources();
		deleteCmdBuffers();
	}

	VERendererRayTracingKHR::secondaryCmdBuf_t VERendererRayTracingKHR::recordRenderpass(
		std::vector<VESubrender *> subRenderers,
		VkFramebuffer *pFrameBuffer,
		uint32_t imageIndex,
		uint32_t numPass,
		VECamera *pCamera,
		VELight *pLight)
	{
		secondaryCmdBuf_t buf;
		buf.pool = getThreadCommandPool();

		vh::vhCmdCreateCommandBuffers(m_device, buf.pool,
			VK_COMMAND_BUFFER_LEVEL_SECONDARY,
			1, &buf.buffer);

		vh::vhCmdBeginCommandBuffer(m_device, VK_NULL_HANDLE, 0, *pFrameBuffer, buf.buffer,
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		// there is only one subrenderer in ray tracing. All object ressources must be loaded at once.
		m_subrenderRT->draw(buf.buffer, imageIndex, numPass, pCamera, pLight);

		vkEndCommandBuffer(buf.buffer);

		return buf;
	}

	/**
		* \brief Create the semaphores and fences for syncing command buffers and swapchain
		*/
	void VERendererRayTracingKHR::createSyncObjects()
	{
		m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT); //for wait for the next swap chain image
		m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT); //for wait for render finished
		m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT); //for wait for at least one image in the swap chain

		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		m_overlaySemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
			{
				assert(false);
				exit(1);
			}
		}
	}

	void VERendererRayTracingKHR::updateCmdBuffers()
	{
		vh::vhDestroyAccelerationStructure(m_device, m_vmaAllocator, m_topLevelAS);
		deleteCmdBuffers();
	};

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

	/**
		* \brief Create a new command buffer and record the whole scene into it, then end it
		*/
	void VERendererRayTracingKHR::recordCmdBuffers()
	{
		VECamera *pCamera;
		VECHECKPOINTER(pCamera = getSceneManagerPointer()->getCamera());

		pCamera->setExtent(getWindowPointer()->getExtent());

		for (uint32_t i = 0; i < m_secondaryBuffers[m_imageIndex].size(); i++)
		{
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
		for (uint32_t i = 0; i < lights.size(); i++)
		{
			VELight *pLight = lights[i];

			//-----------------------------------------------------------------------------------------
			//light pass

			t_now = vh::vhTimeNow();
			{
				auto future = tp->add(&VERendererRayTracingKHR::recordRenderpass, this, m_subrenderers,
					&m_swapChainFramebuffers[m_imageIndex],
					m_imageIndex, i, pCamera, pLight);

				m_secondaryBuffersFutures[m_imageIndex].push_back(std::move(future));
			}
			m_AvgCmdLightTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgCmdLightTime);
		}

		//------------------------------------------------------------------------------------------
		//wait for all threads to finish and copy secondary command buffers into the vector

		m_secondaryBuffers[m_imageIndex].resize(m_secondaryBuffersFutures[m_imageIndex].size());
		for (uint32_t i = 0; i < m_secondaryBuffersFutures[m_imageIndex].size(); i++)
		{
			m_secondaryBuffers[m_imageIndex][i] = m_secondaryBuffersFutures[m_imageIndex][i].get();
		}
		//-----------------------------------------------------------------------------------------
		//create a new primary command buffer and record all secondary buffers into it

		vh::vhCmdCreateCommandBuffers(m_device, m_commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1,
			&m_commandBuffers[m_imageIndex]);

		vh::vhCmdBeginCommandBuffer(m_device, m_commandBuffers[m_imageIndex], (VkCommandBufferUsageFlagBits)0);
		uint32_t bufferIdx = 0;
		VkClearColorValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		VkImageSubresourceRange imageRange = {};
		imageRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageRange.levelCount = 1;
		imageRange.layerCount = 1;

		vkCmdClearColorImage(m_commandBuffers[m_imageIndex], getSwapChainImage(), VK_IMAGE_LAYOUT_GENERAL, &clearColor,
			1, &imageRange);
		for (uint32_t i = 0; i < lights.size(); i++)
		{
			vkCmdExecuteCommands(m_commandBuffers[m_imageIndex], 1,
				&m_secondaryBuffers[m_imageIndex][bufferIdx++].buffer);
		}
		m_AvgRecordTimeOnscreen = vh::vhAverage(vh::vhTimeDuration(t_start), m_AvgRecordTimeOnscreen, 1.0f / m_swapChainImages.size());

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
			m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE,
			&m_imageIndex);

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

		//create tlas if not existing
		//update tlas, if at least one blas is dirty
		updateTLAS();

		if (m_commandBuffers[m_imageIndex] == VK_NULL_HANDLE)
		{
			recordCmdBuffers();
		}

		vh::vhBufTransitionImageLayout(m_device, m_graphicsQueue,
			m_commandPool, //transition the image layout to
			getSwapChainImage(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1,
			1, //VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL);

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
		if (m_subrenderOverlay == nullptr)
			return;
		m_subrenderOverlay->prepareDraw();
	}

	/**
		* \brief Draw the overlay into the current frame buffer
		*/
	void VERendererRayTracingKHR::drawOverlay()
	{
		if (m_subrenderOverlay == nullptr)
			return;

		m_overlaySemaphores[m_currentFrame] = m_subrenderOverlay->draw(m_imageIndex,
			m_renderFinishedSemaphores[m_currentFrame]);
	}

	/**
		* \brief Present the new frame.
		*/
	void VERendererRayTracingKHR::presentFrame()
	{
		vh::vhBufTransitionImageLayout(m_device, m_graphicsQueue,
			m_commandPool, //transition the image layout to
			getSwapChainImage(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1,
			1, //VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		VkResult result = vh::vhRenderPresentResult(m_presentQueue, m_swapChain,
			m_imageIndex, //present it to the swap chain
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

		m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT; //count up the current frame number
	}

	void VERendererRayTracingKHR::updateTLAS()
	{
		bool updateTLAS = false;
		if (m_subrenderRT->getEntities().size())
		{
			std::vector<vh::vhAccelerationStructure> blas;
			for (auto &entity : m_subrenderRT->getEntities())
			{
				blas.push_back(entity->m_AccelerationStructure);
				if (entity->m_AccelerationStructure.isDirty)
				{
					entity->m_AccelerationStructure.isDirty = false;
					updateTLAS = true;
				}
			}
			if (!m_topLevelAS.handleKHR)
			{
				// each entity will create blas, in this step we need to create tlas, which will contain all objects
				vh::vhCreateTopLevelAccelerationStructureKHR(m_device, m_vmaAllocator, m_commandPool, m_graphicsQueue,
					blas, m_topLevelAS);
				m_subrenderRT->updateRTDescriptorSets();
			}
			else if (updateTLAS)
			{
				vh::vhUpdateTopLevelAccelerationStructureKHR(m_device, m_vmaAllocator, m_commandPool, m_graphicsQueue,
					blas, m_topLevelAS);
			}
		}
	}

	void VERendererRayTracingKHR::addEntityToSubrenderer(VEEntity *pEntity)
	{
		veSubrenderType type = VE_SUBRENDERER_TYPE_NONE;

		switch (pEntity->getEntityType())
		{
		case VEEntity::VE_ENTITY_TYPE_NORMAL:
			if (pEntity->m_pMaterial->mapDiffuse != nullptr)
			{
				if (pEntity->m_pMaterial->mapNormal != nullptr)
				{
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
		default:
			return;
		}

		if (type == VE_SUBRENDERER_TYPE_DIFFUSEMAP_NORMALMAP || type == VE_SUBRENDERER_TYPE_DIFFUSEMAP)
		{
			m_subrenderRT->addEntity(pEntity);
		}
	}
} // namespace ve