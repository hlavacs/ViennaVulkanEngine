/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"


const int MAX_FRAMES_IN_FLIGHT = 2;


namespace ve {

	VERendererForward * g_pVERendererForwardSingleton = nullptr;	///<Singleton pointer to the only VERendererForward instance

	VERendererForward::VERendererForward() : VERenderer() {
		g_pVERendererForwardSingleton = this;
	}

	/**
	*
	* \brief Initialize the renderer
	*
	* Create Vulkan objects like physical and logical device, swapchain, renderpasses
	* descriptor set layouts, etc.
	*
	*/
	void VERendererForward::initRenderer() {
		VERenderer::initRenderer();

		const std::vector<const char*> requiredDeviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};

		const std::vector<const char*> requiredValidationLayers = {
			"VK_LAYER_LUNARG_standard_validation"
		};

		vh::vhDevPickPhysicalDevice(getEnginePointer()->getInstance(), m_surface, requiredDeviceExtensions, &m_physicalDevice);
		vh::vhDevCreateLogicalDevice(m_physicalDevice, m_surface, requiredDeviceExtensions, requiredValidationLayers,
									&m_device, &m_graphicsQueue, &m_presentQueue);

		vh::vhMemCreateVMAAllocator(m_physicalDevice, m_device, m_vmaAllocator);

		vh::vhSwapCreateSwapChain(	m_physicalDevice, m_surface, m_device, getWindowPointer()->getExtent(),
									&m_swapChain, m_swapChainImages, m_swapChainImageViews,
									&m_swapChainImageFormat, &m_swapChainExtent);

		//------------------------------------------------------------------------------------------------------------

		//create a command pool
		vh::vhCmdCreateCommandPool(m_physicalDevice, m_device, m_surface, &m_commandPool);

		//------------------------------------------------------------------------------------------------------------
		//create resources for light pass

		m_depthMapFormat = vh::vhDevFindDepthFormat(m_physicalDevice);

		//light render pass
		vh::vhRenderCreateRenderPass( m_device, m_swapChainImageFormat, m_depthMapFormat, &m_renderPass);


		//depth map for light pass
		vh::vhBufCreateDepthResources(	m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, 
										m_swapChainExtent, m_depthMapFormat, &m_depthImage, &m_depthImageAllocation, &m_depthImageView);

		//frame buffers for light pass
		vh::vhBufCreateFramebuffers(m_device, m_swapChainImageViews, m_depthImageView, m_renderPass, m_swapChainExtent, m_swapChainFramebuffers);


		//------------------------------------------------------------------------------------------------------------
		//create rsources for shadow pass

		//shadow render pass
		vh::vhRenderCreateRenderPassShadow( m_device, m_depthMapFormat, &m_renderPassShadow);

		//shadow map
		m_shadowMapExtent = { 2048, 2048 };
		vh::vhBufCreateDepthResources(	m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, m_shadowMapExtent,
										m_depthMapFormat, &m_shadowMap, &m_shadowMapAllocation, &m_shadowMapView);

		//frame buffers for shadow pass
		std::vector<VkImageView> empty;
		vh::vhBufCreateFramebuffers(m_device, empty, m_shadowMapView, m_renderPassShadow, m_shadowMapExtent, m_shadowFramebuffers);

		//------------------------------------------------------------------------------------------------------------
		//per frame resources

		//set 1, binding 0 : UBO per Frame data: camera, light
		vh::vhRenderCreateDescriptorSetLayout(m_device,
											{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
											{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
											&m_descriptorSetLayoutPerFrame);

		//set 2, binding 0, 1 : UBO for shadow data, shadow map + sampler
		vh::vhRenderCreateDescriptorSetLayout(m_device,
											{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
											{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,	VK_SHADER_STAGE_FRAGMENT_BIT },
											&m_descriptorSetLayoutShadow);


		//------------------------------------------------------------------------------------------------------------

		uint32_t maxobjects = 10000;
		vh::vhRenderCreateDescriptorPool(m_device,
										{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER }, 
										{ maxobjects, maxobjects },
										&m_descriptorPool);

		vh::vhRenderCreateDescriptorSets(m_device, (uint32_t)m_swapChainImages.size(), m_descriptorSetLayoutPerFrame, getDescriptorPool(), m_descriptorSetsPerFrame);
		vh::vhRenderCreateDescriptorSets(m_device, (uint32_t)m_swapChainImages.size(), m_descriptorSetLayoutShadow,   getDescriptorPool(), m_descriptorSetsShadow);

		//------------------------------------------------------------------------------------------------------------

		vh::vhBufCreateUniformBuffers(m_vmaAllocator, (uint32_t)m_swapChainImages.size(), (uint32_t)sizeof(veUBOPerFrame), m_uniformBuffersPerFrame, m_uniformBuffersPerFrameAllocation);
		vh::vhBufCreateUniformBuffers(m_vmaAllocator, (uint32_t)m_swapChainImages.size(), (uint32_t)sizeof(veUBOShadow),   m_uniformBuffersShadow,   m_uniformBuffersShadowAllocation);


		//------------------------------------------------------------------------------------------------------------

		createSyncObjects();

		createSubrenderers();
	}

	/**
	* \brief Create and register all known subrenderers for this VERenderer
	*/
	void VERendererForward::createSubrenderers() {
		addSubrenderer(new VESubrenderFW_C1());
		addSubrenderer(new VESubrenderFW_D());
		addSubrenderer(new VESubrenderFW_DN());
		addSubrenderer(new VESubrenderFW_Cubemap());
		
		m_subrenderShadow = new VESubrenderFW_Shadow();
	}


	/**
	* \brief Destroy the swapchain because window resize or close down
	*/
	void VERendererForward::cleanupSwapChain() {
		vkDestroyImageView(m_device, m_depthImageView, nullptr);
		vmaDestroyImage(m_vmaAllocator, m_depthImage, m_depthImageAllocation);

		for (auto framebuffer : m_swapChainFramebuffers) {
			vkDestroyFramebuffer(m_device, framebuffer, nullptr);
		}

		vkDestroyRenderPass(m_device, m_renderPass, nullptr);

		for (auto imageView : m_swapChainImageViews) {
			vkDestroyImageView(m_device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
	}


	/**
	* \brief Close the renderer, destroy all local resources
	*/
	void VERendererForward::closeRenderer() {
		destroySubrenderers();

		cleanupSwapChain();

		vkDestroyFramebuffer(m_device, m_shadowFramebuffers[0], nullptr);

		//destroy shadow map
		vkDestroyImageView(m_device, m_shadowMapView, nullptr);
		vmaDestroyImage(m_vmaAllocator, m_shadowMap, m_shadowMapAllocation);

		vkDestroyRenderPass(m_device, m_renderPassShadow, nullptr);

		//destroy per frame resources
		vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutPerFrame, nullptr);
		for (size_t i = 0; i < m_uniformBuffersPerFrame.size(); i++) {
			vmaDestroyBuffer(m_vmaAllocator, m_uniformBuffersPerFrame[i], m_uniformBuffersPerFrameAllocation[i]);
		}

		vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutShadow, nullptr);
		for (size_t i = 0; i < m_uniformBuffersShadow.size(); i++) {
			vmaDestroyBuffer(m_vmaAllocator, m_uniformBuffersShadow[i], m_uniformBuffersShadowAllocation[i]);
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(m_device, m_commandPool, nullptr);

		vmaDestroyAllocator(m_vmaAllocator);

		vkDestroyDevice(m_device, nullptr);
	}


	/**
	* \brief recreate the swapchain because the window size has changed
	*/
	void VERendererForward::recreateSwapchain() {

		vkDeviceWaitIdle(m_device);

		cleanupSwapChain();

		vh::vhSwapCreateSwapChain(m_physicalDevice, m_surface, m_device, getWindowPointer()->getExtent(),
			&m_swapChain, m_swapChainImages, m_swapChainImageViews,
			&m_swapChainImageFormat, &m_swapChainExtent);

		vh::vhRenderCreateRenderPass( m_device, m_swapChainImageFormat, m_depthMapFormat, &m_renderPass);
		vh::vhBufCreateDepthResources(	m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, m_swapChainExtent,
										m_depthMapFormat, &m_depthImage, &m_depthImageAllocation, &m_depthImageView);

		vh::vhBufCreateFramebuffers(m_device, m_swapChainImageViews, m_depthImageView, m_renderPass, m_swapChainExtent, m_swapChainFramebuffers);

		for (auto pSub : m_subrenderers) pSub->recreateResources();
	}
	

	/**
	* \brief Create the semaphores and fences for syncing command buffers and swapchain
	*/
	void VERendererForward::createSyncObjects() {
		m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT); //for wait for the next swap chain image
		m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT); //for wait for render finished
		m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);		   //for wait for at least one image in the swap chain

		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS ) {
				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
		}
	}

	//--------------------------------------------------------------------------------------------

	/**
	*
	* \brief update the per Frame UBO and its descriptor set
	*
	* \param[in] imageIndex The index of the current swap chain image
	*
	*/
	void VERendererForward::updatePerFrameUBO( uint32_t imageIndex) {

		//---------------------------------------------------------------------------------------

		VECamera * camera = (VECamera*)getSceneManagerPointer()->getEntity(getSceneManagerPointer()->m_cameraName);
		if (camera == nullptr) {
			throw std::runtime_error("Error: did not find camera in Scene Manager!");
		}

		veUBOPerFrame ubo = {};

		//fill in camera data
		ubo.camModel= camera->getWorldTransform();
		ubo.camView = glm::inverse(ubo.camModel);
		ubo.camProj = camera->getProjectionMatrix((float)m_swapChainExtent.width, (float)m_swapChainExtent.height);
		ubo.camProj[1][1] *= -1; //follow Vulkan specification, not GL

		//fill in light data
		ubo.light1.type[0] = -1;
		VELight *plight;
		if (getSceneManagerPointer()->m_lightNames.size() > 0) {
			std::string l1 = *getSceneManagerPointer()->m_lightNames.begin();
			VEEntity *e1 = getSceneManagerPointer()->getEntity(l1);
			if (e1 != nullptr) {
				plight = static_cast<VELight*>(e1);
				plight->fillVhLightStructure(&ubo.light1);
			}
		}

		void* data = nullptr;
		vmaMapMemory(m_vmaAllocator, m_uniformBuffersPerFrameAllocation[imageIndex], &data);
		memcpy(data, &ubo, sizeof(ubo));
		vmaUnmapMemory(m_vmaAllocator, m_uniformBuffersPerFrameAllocation[imageIndex]);

		//update the descriptor set for the per frame data
		vh::vhRenderUpdateDescriptorSet(m_device, m_descriptorSetsPerFrame[imageIndex],
											{ m_uniformBuffersPerFrame[imageIndex] }, //UBOs
											{ sizeof(veUBOPerFrame) },			//UBO sizes
											{ VK_NULL_HANDLE },						//textureImageViews
											{ VK_NULL_HANDLE }						//samplers
											);

		//---------------------------------------------------------------------------------------

		VECamera *pCamShadow = camera->createShadowCameraOrtho(plight);
		veUBOShadow uboShadow = {};
		uboShadow.shadowView = glm::inverse(pCamShadow->getWorldTransform());
		uboShadow.shadowProj = pCamShadow->getProjectionMatrix();

		data=nullptr;
		vmaMapMemory(m_vmaAllocator, m_uniformBuffersShadowAllocation[imageIndex], &data);
		memcpy(data, &uboShadow, sizeof(uboShadow) );
		vmaUnmapMemory(m_vmaAllocator, m_uniformBuffersShadowAllocation[imageIndex]);

		//update the descriptor set for the per frame data
		vh::vhRenderUpdateDescriptorSet(m_device, m_descriptorSetsShadow[imageIndex],
										{ m_uniformBuffersShadow[imageIndex] }, //UBOs
										{ sizeof(veUBOShadow) },			//UBO sizes
										{ VK_NULL_HANDLE },						//textureImageViews
										{ VK_NULL_HANDLE }						//samplers
										);
	}

	/**
	* \brief Draw the frame.
	*
	*- wait for draw completion using a fence, so there is at least one frame in the swapchain
	*- acquire the next image from the swap chain
	*- update all UBOs
	*- get a single time command buffer from the pool, bind pipeline and begin the render pass
	*- loop through all entities and draw them
	*- end the command buffer and submit it
	*- wait for the result and present it
	*/
	void VERendererForward::drawFrame() {
		vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

		//acquire the next image
		VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, std::numeric_limits<uint64_t>::max(),
												m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		updatePerFrameUBO( imageIndex );	//update camera data UBO
		//prepare command buffer for drawing
		VkCommandBuffer commandBuffer = vh::vhCmdBeginSingleTimeCommands(m_device, m_commandPool);

		//-----------------------------------------------------------------------------------------
		//shadow pass

		vh::vhRenderBeginRenderPass(commandBuffer, m_renderPassShadow, m_shadowFramebuffers[0], m_shadowMapExtent);
		//m_subrenderShadow->draw(commandBuffer, imageIndex);
		vkCmdEndRenderPass(commandBuffer);

		//-----------------------------------------------------------------------------------------
		//light pass

		vh::vhRenderBeginRenderPass(commandBuffer, m_renderPass, m_swapChainFramebuffers[imageIndex], m_swapChainExtent);
		for (auto pSub : m_subrenderers) pSub->draw(commandBuffer, imageIndex);
		vkCmdEndRenderPass(commandBuffer);

		vh::vhCmdEndSingleTimeCommands(	m_device, m_graphicsQueue, m_commandPool, commandBuffer,
										m_imageAvailableSemaphores[m_currentFrame], m_renderFinishedSemaphores[m_currentFrame], 
										m_inFlightFences[m_currentFrame]);

	}


	/**
	* \brief Present the new frame.
	*
	* Present the newly drawn frame.
	*/
	void VERendererForward::presentFrame() {
		VkResult result = vh::vhRenderPresentResult(m_presentQueue, m_swapChain, imageIndex,
													m_renderFinishedSemaphores[m_currentFrame]);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
			m_framebufferResized = false;
			recreateSwapchain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

}


