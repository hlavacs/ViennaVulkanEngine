/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VEInclude.h"


const int MAX_FRAMES_IN_FLIGHT = 2;
const uint32_t SHADOW_MAP_DIM = 4096;


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

		m_depthMap = new VETexture("DepthMap");
		m_depthMap->m_format = vh::vhDevFindDepthFormat(m_physicalDevice);
		m_depthMap->m_extent = m_swapChainExtent;

		//light render pass
		vh::vhRenderCreateRenderPass( m_device, m_swapChainImageFormat, m_depthMap->m_format, &m_renderPass);

		//depth map for light pass
		vh::vhBufCreateDepthResources(	m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, 
										m_swapChainExtent, m_depthMap->m_format, &m_depthMap->m_image, &m_depthMap->m_deviceAllocation, &m_depthMap->m_imageView);

		//frame buffers for light pass
		std::vector<VkImageView> depthMaps;
		for (uint32_t i = 0; i < m_swapChainImageViews.size(); i++) depthMaps.push_back(m_depthMap->m_imageView);
		vh::vhBufCreateFramebuffers(m_device, m_swapChainImageViews, depthMaps, m_renderPass, m_swapChainExtent, m_swapChainFramebuffers);

		//------------------------------------------------------------------------------------------------------------
		//create resources for shadow pass

		//shadow render pass
		vh::vhRenderCreateRenderPassShadow( m_device, m_depthMap->m_format, &m_renderPassShadow);

		//shadow maps
		//the outer loop goes over the imageIndex
		//the inner loop is the whole shadow cascade per image index
		for (uint32_t i = 0; i < m_swapChainImageViews.size(); i++) {				//go over number of swapchain images

			std::vector<VETexture*>		shadowMapCascade;							//list of VETextures in a cascade
			std::vector<VkFramebuffer>	frameBufferCascade;

			for (uint32_t j = 0; j < NUM_SHADOW_CASCADE; j++) {
				VkExtent2D extent = { SHADOW_MAP_DIM, SHADOW_MAP_DIM };

				VETexture *pShadowMap = new VETexture("ShadowMap");
				pShadowMap->m_extent = extent;
				pShadowMap->m_format = m_depthMap->m_format;

				vh::vhBufCreateDepthResources(	m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
												extent, pShadowMap->m_format, 
												&pShadowMap->m_image, &pShadowMap->m_deviceAllocation, &pShadowMap->m_imageView);

				vh::vhBufCreateTextureSampler(getRendererPointer()->getDevice(), &pShadowMap->m_sampler);

				shadowMapCascade.push_back(pShadowMap);

				std::vector<VkFramebuffer> frameBuffers;
				vh::vhBufCreateFramebuffers(m_device, { VK_NULL_HANDLE }, { pShadowMap->m_imageView },
											m_renderPassShadow, extent, frameBuffers);

				frameBufferCascade.push_back(frameBuffers[0]);
			}

			m_shadowMaps.push_back(shadowMapCascade);
			m_shadowFramebuffers.push_back(frameBufferCascade);
		}


		//------------------------------------------------------------------------------------------------------------
		//per frame resources
		//set 0...per frame, includes cam and shadow matrices
		//set 1...per object UBO
		//set 2...shadow map
		//set 3...additional per object resources

		//set 0, binding 0 : UBO per Frame data: camera, light, shadow matrices
		vh::vhRenderCreateDescriptorSetLayout(m_device,
											{ 1 },
											{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
											{ VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
											&m_descriptorSetLayoutPerFrame);

		//set 2, binding 0 : shadow map + sampler
		vh::vhRenderCreateDescriptorSetLayout(m_device,
											{ NUM_SHADOW_CASCADE },
											{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
											{ VK_SHADER_STAGE_FRAGMENT_BIT },
											&m_descriptorSetLayoutShadow);

		//------------------------------------------------------------------------------------------------------------
		//create descriptor pool and sets

		uint32_t maxobjects = 10000;
		vh::vhRenderCreateDescriptorPool(m_device,
										{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER }, 
										{ maxobjects, maxobjects },
										&m_descriptorPool);

		vh::vhRenderCreateDescriptorSets(m_device, (uint32_t)m_swapChainImages.size(), m_descriptorSetLayoutPerFrame, getDescriptorPool(), m_descriptorSetsPerFrame);
		vh::vhRenderCreateDescriptorSets(m_device, (uint32_t)m_swapChainImages.size(), m_descriptorSetLayoutShadow,   getDescriptorPool(), m_descriptorSetsShadow);

		//------------------------------------------------------------------------------------------------------------
		//create per frame UBO

		vh::vhBufCreateUniformBuffers(m_vmaAllocator, (uint32_t)m_swapChainImages.size(), (uint32_t)sizeof(vePerFrameData_t), m_uniformBuffersPerFrame, m_uniformBuffersPerFrameAllocation);

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
		addSubrenderer(new VESubrenderFW_Cubemap2());
		addSubrenderer(new VESubrenderFW_Skyplane());

		m_subrenderShadow = new VESubrenderFW_Shadow();
		m_subrenderShadow->initSubrenderer();
	}


	/**
	* \brief Destroy the swapchain because window resize or close down
	*/
	void VERendererForward::cleanupSwapChain() {
		delete m_depthMap;

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

		for (auto framebufferList : m_shadowFramebuffers) {
			for (auto framebuffer : framebufferList) {
				vkDestroyFramebuffer(m_device, framebuffer, nullptr);
			}
		}

		//destroy shadow maps
		for (auto shadowMapList : m_shadowMaps) {
			for (auto pShadowMap : shadowMapList) {
				delete pShadowMap;
			}
		}
		vkDestroyRenderPass(m_device, m_renderPassShadow, nullptr);

		//destroy per frame resources
		vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutPerFrame, nullptr);
		for (size_t i = 0; i < m_uniformBuffersPerFrame.size(); i++) {
			vmaDestroyBuffer(m_vmaAllocator, m_uniformBuffersPerFrame[i], m_uniformBuffersPerFrameAllocation[i]);
		}

		vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutShadow, nullptr);

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

		m_depthMap = new VETexture("DepthMap");
		m_depthMap->m_format = vh::vhDevFindDepthFormat(m_physicalDevice);
		m_depthMap->m_extent = m_swapChainExtent;

		vh::vhRenderCreateRenderPass( m_device, m_swapChainImageFormat, m_depthMap->m_format, &m_renderPass);
		vh::vhBufCreateDepthResources(	m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, m_swapChainExtent,
										m_depthMap->m_format, &m_depthMap->m_image, &m_depthMap->m_deviceAllocation, &m_depthMap->m_imageView);

		std::vector<VkImageView> depthMaps;
		for (uint32_t i = 0; i < m_swapChainImageViews.size(); i++) depthMaps.push_back(m_depthMap->m_imageView);
		vh::vhBufCreateFramebuffers(m_device, m_swapChainImageViews, depthMaps, m_renderPass, m_swapChainExtent, m_swapChainFramebuffers);

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
	void VERendererForward::updatePerFrameUBO(uint32_t imageIndex) {

		//---------------------------------------------------------------------------------------

		vePerFrameData_t ubo = {};

		//fill in camera data
		VECamera * camera = getSceneManagerPointer()->getCamera();
		camera->fillCameraStructure(&ubo.camera);

		//fill in light data
		VELight *plight = getSceneManagerPointer()->getLights()[0];
		plight->fillLightStructure(&ubo.light);

		//fill in shadow data
		std::vector<float> limits = {0.0f, 0.05f, 0.15f, 0.50f, 1.0f};

		for (uint32_t i = 0; i < NUM_SHADOW_CASCADE; i++) {
			ubo.shadow[i].limits[0] = limits[i];
			ubo.shadow[i].limits[1] = limits[i+1];
			VECamera *pCamShadow =
				camera->createShadowCamera(plight, limits[i], limits[i+1]);
			pCamShadow->fillShadowStructure(&ubo.shadow[i]);
			delete pCamShadow;
		}

		void* data = nullptr;
		vmaMapMemory(m_vmaAllocator, m_uniformBuffersPerFrameAllocation[imageIndex], &data);
		memcpy(data, &ubo, sizeof(ubo));
		vmaUnmapMemory(m_vmaAllocator, m_uniformBuffersPerFrameAllocation[imageIndex]);

		//update the descriptor set for the per frame data
		vh::vhRenderUpdateDescriptorSet(m_device, m_descriptorSetsPerFrame[imageIndex],
										{ m_uniformBuffersPerFrame[imageIndex] },	//UBOs
										{ sizeof(vePerFrameData_t) },				//UBO sizes
										{ {VK_NULL_HANDLE} },						//textureImageViews
										{ {VK_NULL_HANDLE} });						//samplers
	

		//update the descriptor set for light pass - array of shadow maps
		std::vector<std::vector<VkImageView>>	imageViews;
		std::vector<std::vector<VkSampler>>		samplers;
		imageViews.resize(1);
		samplers.resize(1);
		for (uint32_t i = 0; i < NUM_SHADOW_CASCADE; i++) {
			imageViews[0].push_back(m_shadowMaps[imageIndex][i]->m_imageView);
			samplers[0].push_back  (m_shadowMaps[imageIndex][i]->m_sampler);
		}

		vh::vhRenderUpdateDescriptorSet(m_device, m_descriptorSetsShadow[imageIndex],
										{ VK_NULL_HANDLE },			//UBOs
										{ 0				},			//UBO sizes
										{ imageViews },				//textureImageViews
										{ samplers }				//samplers
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
		//shadow passes

		std::vector<VkClearValue> clearValues = {};
		VkClearValue cv;
		cv.depthStencil = { 1.0f, 0 };
		clearValues.push_back(cv);

		for ( unsigned i = 0; i < NUM_SHADOW_CASCADE; i++) {
			vh::vhRenderBeginRenderPass(commandBuffer, m_renderPassShadow,
										m_shadowFramebuffers[imageIndex][i], 
										clearValues, m_shadowMaps[imageIndex][i]->m_extent);

			m_subrenderShadow->bindPipeline(commandBuffer);
			vkCmdPushConstants(commandBuffer, m_subrenderShadow->getPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(unsigned), &i);
			m_subrenderShadow->draw(commandBuffer, imageIndex);
			vkCmdEndRenderPass(commandBuffer);
		}

		//-----------------------------------------------------------------------------------------
		//light pass

		vh::vhRenderBeginRenderPass(commandBuffer, m_renderPass, m_swapChainFramebuffers[imageIndex], m_swapChainExtent);
		for (auto pSub : m_subrenderers) {
			pSub->bindPipeline(commandBuffer);
			pSub->draw(commandBuffer, imageIndex);
		}
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


