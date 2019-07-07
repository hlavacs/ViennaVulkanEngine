/**
* The Vienna Vulkan Engine
*
* (c) bei Alexander Fomin, University of Vienna
* 
*/



#include "VERendererDeferred.h"
#include "VESubrenderDF_C1.h"
#include "VESubrenderDF_D.h"
#include "VESubrenderDF_DN.h"
#include "VESubrenderDF_Cubemap.h"
#include "VESubrenderDF_Cubemap2.h"
#include "VESubrenderDF_Shadow.h"
#include "VESubrenderDF_Skyplane.h"
#include "VESubrenderDF_Composer.h"


namespace ve {

	VERendererDeferred* g_pVERendererDeferredSingleton = nullptr;	///<Singleton pointer to the only VERendererDeferred instance

	VERendererDeferred::VERendererDeferred() : VERenderer() {
		g_pVERendererDeferredSingleton = this;
	}

	/**
	*
	* \brief Initialize the renderer
	*
	* Create Vulkan objects like physical and logical device, swapchain, renderpasses
	* descriptor set layouts, etc.
	*
	*/
	void VERendererDeferred::initRenderer() {
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

        vh::vhSwapCreateSwapChain(m_physicalDevice, m_surface, m_device, getWindowPointer()->getExtent(),
            &m_swapChain, m_swapChainImages, m_swapChainImageViews,
            &m_swapChainImageFormat, &m_swapChainExtent);

		//------------------------------------------------------------------------------------------------------------

		//create a command pool
		vh::vhCmdCreateCommandPool(m_physicalDevice, m_device, m_surface, &m_commandPool);

        m_commandBuffers.resize(m_swapChainImages.size());
        for(uint32_t i = 0; i < m_swapChainImages.size(); i++) m_commandBuffers[i] = VK_NULL_HANDLE;

		//------------------------------------------------------------------------------------------------------------
		//create resources for g-buffer pass

		m_depthMap = new VETexture("DepthMap");
		m_depthMap->m_format = vh::vhDevFindDepthFormat(m_physicalDevice);
		m_depthMap->m_extent = m_swapChainExtent;

        m_positionMap = new VETexture("Position");
        m_positionMap->m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        m_positionMap->m_extent = m_swapChainExtent;

        m_normalMap = new VETexture("Normal");
        m_normalMap->m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        m_normalMap->m_extent = m_swapChainExtent;

        m_albedoMap = new VETexture("Albedo");
        m_albedoMap->m_format = VK_FORMAT_R8G8B8A8_UNORM;
        m_albedoMap->m_extent = m_swapChainExtent;

		//g-buffer render pass
		vh::vhRenderCreateRenderPassGBuffer(m_device, m_swapChainImageFormat, m_depthMap->m_format, VK_ATTACHMENT_LOAD_OP_CLEAR, &m_renderPassClear);
        vh::vhRenderCreateRenderPassGBuffer(m_device, m_swapChainImageFormat, m_depthMap->m_format, VK_ATTACHMENT_LOAD_OP_LOAD, &m_renderPassLoad);

		//buffers for g-buffer
		vh::vhBufCreateGBufferResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
			m_swapChainExtent, m_positionMap->m_format, &m_positionMap->m_image, &m_positionMap->m_deviceAllocation, &m_positionMap->m_imageView);
        vh::vhBufCreateGBufferResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
            m_swapChainExtent, m_normalMap->m_format, &m_normalMap->m_image, &m_normalMap->m_deviceAllocation, &m_normalMap->m_imageView);
        vh::vhBufCreateGBufferResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
            m_swapChainExtent, m_albedoMap->m_format, &m_albedoMap->m_image, &m_albedoMap->m_deviceAllocation, &m_albedoMap->m_imageView);

		//depth map for g-buffer
		vh::vhBufCreateDepthResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
			m_swapChainExtent, m_depthMap->m_format, &m_depthMap->m_image, &m_depthMap->m_deviceAllocation, &m_depthMap->m_imageView);
	
		//frame buffers
        std::vector<VkImageView> postionMaps;
        std::vector<VkImageView> normalMaps;
        std::vector<VkImageView> albedoMaps;
        std::vector<VkImageView> depthMaps;
        for(uint32_t i = 0; i < m_swapChainImageViews.size(); i++) {
            postionMaps.push_back(m_positionMap->m_imageView);
            normalMaps.push_back(m_normalMap->m_imageView);
            albedoMaps.push_back(m_albedoMap->m_imageView);
            depthMaps.push_back(m_depthMap->m_imageView);
        }
        // setupFrameBuffer() ref
		vh::vhBufCreateFramebuffersGBuffer(m_device, m_swapChainImageViews, postionMaps,
                                           normalMaps, albedoMaps, depthMaps, m_renderPassClear,
                                           m_swapChainExtent, m_swapChainFramebuffers);

        
		//------------------------------------------------------------------------------------------------------------
		//create resources for shadow pass

		//shadow render pass
		vh::vhRenderCreateRenderPassShadow(m_device, m_depthMap->m_format, &m_renderPassShadow);

        //shadow maps
        m_shadowMaps.resize(m_swapChainImageViews.size());
        m_shadowFramebuffers.resize(m_swapChainImageViews.size());
        for(uint32_t i = 0; i < m_swapChainImageViews.size(); i++) {
            for(uint32_t j = 0; j < NUM_SHADOW_CASCADE; j++) {						//point light has 6 shadow maps, thats the max
                VkExtent2D extent = { SHADOW_MAP_DIM, SHADOW_MAP_DIM };

                VETexture *pShadowMap = new VETexture("ShadowMap");
                pShadowMap->m_extent = extent;
                pShadowMap->m_format = m_depthMap->m_format;

                //create the depth images
                vh::vhBufCreateDepthResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
                    extent, pShadowMap->m_format,
                    &pShadowMap->m_image, &pShadowMap->m_deviceAllocation, &pShadowMap->m_imageView);

                vh::vhBufCreateTextureSampler(getRendererPointer()->getDevice(), &pShadowMap->m_sampler);

                m_shadowMaps[i].push_back(pShadowMap);

                //create the framebuffers holding only the depth images for the shadow maps
                std::vector<VkFramebuffer> frameBuffers;
                vh::vhBufCreateFramebuffers(m_device, { VK_NULL_HANDLE }, { pShadowMap->m_imageView },
                    m_renderPassShadow, extent, frameBuffers);

                m_shadowFramebuffers[i].push_back(frameBuffers[0]);
            }
        }

        //------------------------------------------------------------------------------------------------------------
        //create descriptor pool, layout and sets

        uint32_t maxobjects = 10000;
        vh::vhRenderCreateDescriptorPool(m_device,
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER , VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER , VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT },
            { maxobjects, maxobjects , maxobjects },
            &m_descriptorPool);


        //set 0...cam UBO
        //set 1...light UBO
        //set 2...shadow maps
        //set 3...per object UBO
        //set 4...additional per object resources

        //set 2, binding 0 : shadow map + sampler
        vh::vhRenderCreateDescriptorSetLayout(m_device,
            { NUM_SHADOW_CASCADE },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
            { VK_SHADER_STAGE_FRAGMENT_BIT },
            &m_descriptorSetLayoutShadow);

        //set 3, binding 0 : UBO per scene object: camera, light, entity
        vh::vhRenderCreateDescriptorSetLayout(getDevice(),
            { 1 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
            { VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT , },
            &m_descriptorSetLayoutPerObject);
        

        //------------------------------------------------------------------------------------------------------------
        //create descriptor sets
		//vh::vhRenderCreateDescriptorSets(m_device, (uint32_t)m_swapChainImages.size(), m_descriptorSetLayoutPerFrame, getDescriptorPool(), m_descriptorSetsPerFrame);
		vh::vhRenderCreateDescriptorSets(m_device, (uint32_t)m_swapChainImages.size(), m_descriptorSetLayoutShadow, getDescriptorPool(), m_descriptorSetsShadow);

        //update the descriptor set for light pass - array of shadow maps
        for(uint32_t i = 0; i < m_swapChainImageViews.size(); i++) {

            std::vector<std::vector<VkImageView>>	imageViews;
            std::vector<std::vector<VkSampler>>		samplers;
            imageViews.resize(1);
            samplers.resize(1);

            for(uint32_t j = 0; j < NUM_SHADOW_CASCADE; j++) {
                imageViews[0].push_back(m_shadowMaps[i][j]->m_imageView);
                samplers[0].push_back(m_shadowMaps[i][j]->m_sampler);
            }

            vh::vhRenderUpdateDescriptorSet(m_device, m_descriptorSetsShadow[i],
                { VK_NULL_HANDLE },		//UBOs
                { 0 },					//UBO sizes
                { imageViews },			//textureImageViews
                { samplers }			//samplers
            );

        }
		//------------------------------------------------------------------------------------------------------------

		createSyncObjects();

		createSubrenderers();
	}

	/**
	* \brief Create and register all known subrenderers for this VERenderer
	*/
	void VERendererDeferred::createSubrenderers() {
		addSubrenderer(new VESubrenderDF_C1());
		addSubrenderer(new VESubrenderDF_D());
		addSubrenderer(new VESubrenderDF_DN());
		addSubrenderer(new VESubrenderDF_Cubemap());
		addSubrenderer(new VESubrenderDF_Cubemap2());
		addSubrenderer(new VESubrenderDF_Skyplane());
        addSubrenderer(new VESubrenderDF_Shadow());
        //addSubrenderer(new VESubrenderFW_Nuklear());

		m_subrenderComposer = new VESubrenderDF_Composer();
		m_subrenderComposer->initSubrenderer();

        m_subrenderNuklear = new VESubrenderFW_Nuklear();
	}


	/**
	* \brief Destroy the swapchain because window resize or close down
	*/
	void VERendererDeferred::cleanupSwapChain() {
		delete m_depthMap;
        delete m_positionMap;
        delete m_normalMap;
        delete m_albedoMap;

		for(auto framebuffer : m_swapChainFramebuffers) {
			vkDestroyFramebuffer(m_device, framebuffer, nullptr);
		}

        vkDestroyRenderPass(m_device, m_renderPassClear, nullptr);
        vkDestroyRenderPass(m_device, m_renderPassLoad, nullptr);

		for(auto imageView : m_swapChainImageViews) {
			vkDestroyImageView(m_device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
	}

    void VERendererDeferred::destroySubrenderers() {
        VERenderer::destroySubrenderers();

        if(m_subrenderComposer != nullptr) {
            m_subrenderComposer->closeSubrenderer();
            delete m_subrenderComposer;
        }
    }
	/**
	* \brief Close the renderer, destroy all local resources
	*/
	void VERendererDeferred::closeRenderer() {
		destroySubrenderers();

        deleteCmdBuffers();

		cleanupSwapChain();

		for(auto framebufferList : m_shadowFramebuffers) {
			for(auto framebuffer : framebufferList) {
				vkDestroyFramebuffer(m_device, framebuffer, nullptr);
			}
		}

        //destroy shadow maps
        for(auto pShadowMapList : m_shadowMaps) {
            for(auto pS : pShadowMapList) {
                delete pS;
            }
        }
        vkDestroyRenderPass(m_device, m_renderPassShadow, nullptr);

        //destroy per frame resources
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutPerObject, nullptr);
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayoutShadow, nullptr);

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
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
	void VERendererDeferred::recreateSwapchain() {

		vkDeviceWaitIdle(m_device);

		cleanupSwapChain();

        vh::vhSwapCreateSwapChain(m_physicalDevice, m_surface, m_device, getWindowPointer()->getExtent(),
            &m_swapChain, m_swapChainImages, m_swapChainImageViews,
            &m_swapChainImageFormat, &m_swapChainExtent);

        //------------------------------------------------------------------------------------------------------------
        //create resources for g-buffer pass

        m_depthMap = new VETexture("DepthMap");
        m_depthMap->m_format = vh::vhDevFindDepthFormat(m_physicalDevice);
        m_depthMap->m_extent = m_swapChainExtent;

        m_positionMap = new VETexture("Position");
        m_positionMap->m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        m_positionMap->m_extent = m_swapChainExtent;

        m_normalMap = new VETexture("Normal");
        m_normalMap->m_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        m_normalMap->m_extent = m_swapChainExtent;

        m_albedoMap = new VETexture("Albedo");
        m_albedoMap->m_format = VK_FORMAT_R8G8B8A8_UNORM;
        m_albedoMap->m_extent = m_swapChainExtent;

        //g-buffer render pass
        vh::vhRenderCreateRenderPassGBuffer(m_device, m_swapChainImageFormat, m_depthMap->m_format, VK_ATTACHMENT_LOAD_OP_CLEAR, &m_renderPassClear);
        vh::vhRenderCreateRenderPassGBuffer(m_device, m_swapChainImageFormat, m_depthMap->m_format, VK_ATTACHMENT_LOAD_OP_LOAD, &m_renderPassLoad);

        //buffers for g-buffer
        vh::vhBufCreateGBufferResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
            m_swapChainExtent, m_positionMap->m_format, &m_positionMap->m_image, &m_positionMap->m_deviceAllocation, &m_positionMap->m_imageView);
        vh::vhBufCreateGBufferResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
            m_swapChainExtent, m_normalMap->m_format, &m_normalMap->m_image, &m_normalMap->m_deviceAllocation, &m_normalMap->m_imageView);
        vh::vhBufCreateGBufferResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
            m_swapChainExtent, m_albedoMap->m_format, &m_albedoMap->m_image, &m_albedoMap->m_deviceAllocation, &m_albedoMap->m_imageView);

        //depth map for g-buffer
        vh::vhBufCreateDepthResources(m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool,
            m_swapChainExtent, m_depthMap->m_format, &m_depthMap->m_image, &m_depthMap->m_deviceAllocation, &m_depthMap->m_imageView);

        //frame buffers
        std::vector<VkImageView> depthMaps;
        std::vector<VkImageView> postionMaps;
        std::vector<VkImageView> normalMaps;
        std::vector<VkImageView> albedoMaps;
        for(uint32_t i = 0; i < m_swapChainImageViews.size(); i++) {
            postionMaps.push_back(m_positionMap->m_imageView);
            normalMaps.push_back(m_normalMap->m_imageView);
            albedoMaps.push_back(m_albedoMap->m_imageView);
            depthMaps.push_back(m_depthMap->m_imageView);
        }
        // setupFrameBuffer() ref
        vh::vhBufCreateFramebuffersGBuffer(m_device, m_swapChainImageViews, postionMaps,
            normalMaps, albedoMaps, depthMaps, m_renderPassClear,
            m_swapChainExtent, m_swapChainFramebuffers);

        for(auto pSub : m_subrenderers) pSub->recreateResources();
        
        m_subrenderComposer->recreateResources();

        deleteCmdBuffers();
	}


	/**
	* \brief Create the semaphores and fences for syncing command buffers and swapchain
	*/
	void VERendererDeferred::createSyncObjects() {
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT); //for wait for the next swap chain image
        m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT); //for wait for render finished
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);		   //for wait for at least one image in the swap chain

        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        m_overlaySemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
                getEnginePointer()->fatalError("Failed to create synchronization objects for a frame!");
            }
        }
	}

    //--------------------------------------------------------------------------------------------

    /**
    * \brief Delete all command buffers and set them to VK_NULL_HANDLE, so next time they have to be
    * created and recorded again
    */
    void VERendererDeferred::deleteCmdBuffers() {
        for(uint32_t i = 0; i < m_commandBuffers.size(); i++) {
            if(m_commandBuffers[i] != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(m_device, m_commandPool, 1, &m_commandBuffers[i]);
                m_commandBuffers[i] = VK_NULL_HANDLE;
            }
        }
    }

    /**
    * \brief Create a new command buffer and record the whole scene into it, then end it
    */
    void VERendererDeferred::recordCmdBuffers() {
        VECamera *pCamera = getSceneManagerPointer()->getCamera();
        pCamera->setExtent(getWindowPointer()->getExtent());

        vh::vhCmdCreateCommandBuffers(m_device, m_commandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            1, &m_commandBuffers[imageIndex]);

        vh::vhCmdBeginCommandBuffer(m_device, m_commandBuffers[imageIndex], VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);

        //-----------------------------------------------------------------------------------------
        //set clear values for shadow and light passes

        std::vector<VkClearValue> clearValuesShadow = {};	//shadow map should be cleared every time
        VkClearValue cv;
        cv.depthStencil = { 1.0f, 0 };
        clearValuesShadow.push_back(cv);

        std::vector<VkClearValue> clearValuesLight = {};//render target and depth buffer should be cleared only first time
        VkClearValue cv1, cv2;
        cv1.color = { 0.0f, 0.0f, 0.0f, 1.0f };
        clearValuesLight.push_back(cv1);
        clearValuesLight.push_back(cv1);
        clearValuesLight.push_back(cv1);
        clearValuesLight.push_back(cv1);
        cv2.depthStencil = { 1.0f, 0 };
        clearValuesLight.push_back(cv2);

        vh::vhRenderBeginRenderPass(m_commandBuffers[imageIndex],
            m_renderPassClear,
            m_swapChainFramebuffers[imageIndex],
            clearValuesLight,
            m_swapChainExtent);

        for(auto pSub : m_subrenderers) {
            pSub->prepareDraw();
            pSub->draw(m_commandBuffers[imageIndex], imageIndex, 0, pCamera, getSceneManagerPointer()->getLights()[0], m_descriptorSetsShadow);
        }

        vkCmdEndRenderPass(m_commandBuffers[imageIndex]);
        clearValuesLight.clear();		//since we blend the images onto each other, do not clear them for passes 2 and further

        //go through all active lights in the scene

        std::chrono::high_resolution_clock::time_point t_now;

        for(uint32_t i = 0; i < getSceneManagerPointer()->getLights().size(); i++) {
            VELight * pLight = getSceneManagerPointer()->getLights()[i];

            //-----------------------------------------------------------------------------------------
            //shadow passes

            t_now = vh::vhTimeNow();
            {
                for(unsigned i = 0; i < pLight->m_shadowCameras.size(); i++) {

                    vh::vhRenderBeginRenderPass(m_commandBuffers[imageIndex],
                        m_renderPassShadow,
                        m_shadowFramebuffers[imageIndex][i],
                        clearValuesShadow,
                        m_shadowMaps[0][i]->m_extent);	//all shadow maps have the same extent

                    m_subrenderShadow->draw(m_commandBuffers[imageIndex], imageIndex, i, pLight->m_shadowCameras[i], pLight, {});

                    vkCmdEndRenderPass(m_commandBuffers[imageIndex]);
                }
            }
            m_AvgCmdShadowTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgCmdShadowTime);

            //-----------------------------------------------------------------------------------------
            //light pass

            t_now = vh::vhTimeNow();
            {
                vh::vhRenderBeginRenderPass(m_commandBuffers[imageIndex],
                    m_renderPassLoad,
                    m_swapChainFramebuffers[imageIndex],
                    clearValuesLight,
                    m_swapChainExtent);

                vkCmdNextSubpass(m_commandBuffers[imageIndex], VK_SUBPASS_CONTENTS_INLINE);

                //-----------------------------------------------------------------------------------------
                //composer  pass
                m_subrenderComposer->draw(m_commandBuffers[imageIndex], imageIndex, i, pCamera, pLight, m_descriptorSetsShadow);
                //m_subrenderNuklear->prepareDraw();
                //m_subrenderNuklear->draw(m_commandBuffers[imageIndex], imageIndex, i, pCamera, pLight, m_descriptorSetsShadow);

                vkCmdEndRenderPass(m_commandBuffers[imageIndex]);
            }
            m_AvgCmdLightTime = vh::vhAverage(vh::vhTimeDuration(t_now), m_AvgCmdLightTime);

            clearValuesLight.clear();		//since we blend the images onto each other, do not clear them for passes 2 and further*/
        }

        vkEndCommandBuffer(m_commandBuffers[imageIndex]);

        m_overlaySemaphores[m_currentFrame] = m_renderFinishedSemaphores[m_currentFrame];
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
	void VERendererDeferred::drawFrame() {
        vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

        //acquire the next image
        VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, std::numeric_limits<uint64_t>::max(),
            m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

        if(result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            getEnginePointer()->fatalError("Failed to acquire swap chain image!");
        }

        if(m_commandBuffers[imageIndex] == VK_NULL_HANDLE) {
            recordCmdBuffers();
        }

        //submit the command buffers
        vh::vhCmdSubmitCommandBuffer(m_device, m_graphicsQueue, m_commandBuffers[imageIndex],
            m_imageAvailableSemaphores[m_currentFrame],
            m_renderFinishedSemaphores[m_currentFrame],
            m_inFlightFences[m_currentFrame]);
	}

    /**
    * \brief Prepare to creat an overlay, e.g. initialize the next frame
    */
    void VERendererDeferred::prepareOverlay() {
        if(m_subrenderOverlay == nullptr) return;
        m_subrenderOverlay->prepareDraw();
    }


    /**
    * \brief Draw the overlay into the current frame buffer
    */
    void VERendererDeferred::drawOverlay() {
        if(m_subrenderOverlay == nullptr) return;

        m_overlaySemaphores[m_currentFrame] = m_subrenderOverlay->draw(imageIndex, m_renderFinishedSemaphores[m_currentFrame]);
    }


	/**
	* \brief Present the new frame.
	*
	* Present the newly drawn frame.
	*/
	void VERendererDeferred::presentFrame() {
        vh::vhBufTransitionImageLayout(m_device, m_graphicsQueue, m_commandPool,				//transition the image layout to 
            getSwapChainImage(), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1, 1,		//VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		VkResult result = vh::vhRenderPresentResult(m_presentQueue, m_swapChain, imageIndex,    //present it to the swap chain
			m_renderFinishedSemaphores[m_currentFrame]);

		if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
			m_framebufferResized = false;
			recreateSwapchain();
		}
		else if(result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

}


