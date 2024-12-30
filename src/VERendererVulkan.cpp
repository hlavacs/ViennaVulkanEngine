#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// Vulkan Renderer

    RendererVulkan::RendererVulkan(std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName) {

        engine.RegisterCallback( { 
			{this,      0, "EXTENSIONS", [this](Message message){this->OnExtensions(message);} },
			{this,   1000, "INIT", [this](Message message){this->OnInit(message);} }, 
			{this,      0, "PREPARE_NEXT_FRAME", [this](Message message){this->OnPrepareNextFrame(message);} },
			{this,      0, "RECORD_NEXT_FRAME", [this](Message message){this->OnRecordNextFrame(message);} },
			{this,      0, "RENDER_NEXT_FRAME", [this](Message message){this->OnRenderNextFrame(message);} },

			{this,      0, "OBJECT_CREATE", [this](Message message){this->OnObjectCreate(message);} },
			{this,      0, "TEXTURE_CREATE",   [this](Message message){this->OnTextureCreate(message);} },
			{this,      0, "TEXTURE_DESTROY",  [this](Message message){this->OnTextureDestroy(message);} },
			{this,      0, "GEOMETRY_CREATE",  [this](Message message){this->OnGeometryCreate(message);} },
			{this,      0, "GEOMETRY_DESTROY", [this](Message message){this->OnGeometryDestroy(message);} },

			{this,   2000, "QUIT", [this](Message message){this->OnQuit(message);} },
		} );
    }

    RendererVulkan::~RendererVulkan() {}

    void RendererVulkan::OnExtensions(Message message) {
		auto msg = message.template GetData<MsgExtensions>();
		m_instanceExtensions.insert(m_instanceExtensions.end(), msg.m_instExt.begin(), msg.m_instExt.end());
		m_deviceExtensions.insert(m_deviceExtensions.end(), msg.m_devExt.begin(), msg.m_devExt.end());
	}

    void RendererVulkan::OnInit(Message message) {
    	m_windowSDL = (WindowSDL*)m_window;
		if (m_engine.GetDebug()) {
            m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

    	vh::createInstance( m_validationLayers, m_instanceExtensions, m_instance);
		if (m_engine.GetDebug()) {
	        vh::setupDebugMessenger(m_instance, m_debugMessenger);
		}

		if (SDL_Vulkan_CreateSurface(m_windowSDL->GetSDLWindow(), m_instance, &m_surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }

        vh::pickPhysicalDevice(m_instance, m_deviceExtensions, m_surface, m_physicalDevice);
        vh::createLogicalDevice(m_surface, m_physicalDevice, m_queueFamilies, m_validationLayers, m_deviceExtensions, m_device, m_graphicsQueue, m_presentQueue);
        vh::initVMA(m_instance, m_physicalDevice, m_device, m_vmaAllocator);  
        vh::createSwapChain(m_windowSDL->GetSDLWindow(), m_surface, m_physicalDevice, m_device, m_swapChain);
        vh::createImageViews(m_device, m_swapChain);
        vh::createRenderPassClear(m_physicalDevice, m_device, m_swapChain, true, m_renderPass);
        
		vh::createDescriptorSetLayout(m_device, m_descriptorSetLayouts);

        vh::createGraphicsPipeline(m_device, m_renderPass, m_descriptorSetLayouts, m_graphicsPipeline);
        vh::createCommandPool(m_surface, m_physicalDevice, m_device, m_commandPool);
        vh::createDepthResources(m_physicalDevice, m_device, m_vmaAllocator, m_swapChain, m_depthImage);
        vh::createFramebuffers(m_device, m_swapChain, m_depthImage, m_renderPass);

        vh::createCommandBuffers(m_device, m_commandPool, m_commandBuffers);
        vh::createDescriptorPool(m_device, 1000, m_descriptorPool);
        vh::createSemaphores(m_device, 3, m_imageAvailableSemaphores, m_semaphores);
		vh::createFences(m_device, MAX_FRAMES_IN_FLIGHT, m_fences);
    }

    void RendererVulkan::OnPrepareNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return;

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		m_commandBuffersSubmit.clear();

		vkWaitForFences(m_device, 1, &m_fences[m_currentFrame], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain.m_swapChain, UINT64_MAX,
                            m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
            recreateSwapChain(m_windowSDL->GetSDLWindow(), m_surface, m_physicalDevice, m_device, m_vmaAllocator, m_swapChain, m_depthImage, m_renderPass);
            return;
        } else assert (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
    }


    void RendererVulkan::OnRecordNextFrame(Message message) {
		if(m_windowSDL->GetIsMinimized()) return;

        vkResetCommandBuffer(m_commandBuffers[m_currentFrame],  0);

		vh::startRecordCommandBuffer(m_commandBuffers[m_currentFrame], m_imageIndex, 
			m_swapChain, m_renderPass, m_graphicsPipeline, 
			true, ((WindowSDL*)m_window)->GetClearColor(), m_currentFrame);

		vh::endRecordCommandBuffer(m_commandBuffers[m_currentFrame]);

		SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
	}

    void RendererVulkan::OnRenderNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return;
		VkResult result;
        
		size_t size = m_commandBuffersSubmit.size();
		if( size > m_semaphores.size() ) {
			vh::createSemaphores(m_device, size, m_imageAvailableSemaphores, m_semaphores);
		}

        vkResetFences(m_device, 1, &m_fences[m_currentFrame]);

		VkSemaphore waitSemaphore = m_imageAvailableSemaphores[m_currentFrame];
		VkSemaphore signalSemaphore;

		for( int i = 0; i < size; i++ ) {
			VkCommandBuffer commandBuffer = m_commandBuffersSubmit[i];
			signalSemaphore = m_semaphores[i].m_renderFinishedSemaphores[m_currentFrame];
			VkSubmitInfo submitInfo{};
	        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	        submitInfo.waitSemaphoreCount = 1;
	        submitInfo.pWaitSemaphores = &waitSemaphore;
	        submitInfo.pWaitDstStageMask = waitStages;
	        submitInfo.commandBufferCount = 1;
	        submitInfo.pCommandBuffers = &commandBuffer;
	        submitInfo.signalSemaphoreCount = 1;
	        submitInfo.pSignalSemaphores = &signalSemaphore;
			VkFence fence = VK_NULL_HANDLE;
			if( i== size-1 ) fence = m_fences[m_currentFrame];
	        if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
	            throw std::runtime_error("failed to submit draw command buffer!");
	        }
			waitSemaphore = signalSemaphore;
		}

   		vh::transitionImageLayout(m_device, m_graphicsQueue, m_commandPool, 
			m_swapChain.m_swapChainImages[m_imageIndex], m_swapChain.m_swapChainImageFormat, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &signalSemaphore;
        VkSwapchainKHR swapChains[] = {m_swapChain.m_swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &m_imageIndex;
        result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
            m_framebufferResized = false;
            recreateSwapChain(m_windowSDL->GetSDLWindow(), m_surface, m_physicalDevice, m_device, m_vmaAllocator, m_swapChain, m_depthImage, m_renderPass);
        } else assert(result == VK_SUCCESS);
    }
    
    void RendererVulkan::OnQuit(Message message) {

        vkDeviceWaitIdle(m_device);

        vh::cleanupSwapChain(m_device, m_vmaAllocator, m_swapChain, m_depthImage);

        vkDestroyPipeline(m_device, m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_graphicsPipeline.m_pipelineLayout, nullptr);

        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

		for( decltype(auto) texture : m_registry.template GetView<vh::Texture&>() ) {
			vkDestroySampler(m_device, texture.m_textureSampler, nullptr);
        	vkDestroyImageView(m_device, texture.m_textureImageView, nullptr);
	        vh::destroyImage(m_device, m_vmaAllocator, texture.m_textureImage, texture.m_textureImageAllocation);
		}

		for( auto layout : m_descriptorSetLayouts.m_descriptorSetLayouts ) {
			vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
		}

		for( auto geometry : m_registry.template GetView<vh::Geometry&>() ) {
	        vh::destroyBuffer(m_device, m_vmaAllocator, geometry.m_indexBuffer, geometry.m_indexBufferAllocation);
	        vh::destroyBuffer(m_device, m_vmaAllocator, geometry.m_vertexBuffer, geometry.m_vertexBufferAllocation);
		}

		for( auto ubo : m_registry.template GetView<vh::UniformBuffers&>() ) {
			vh::destroyBuffer2(m_device, m_vmaAllocator, ubo);
		}

        vkDestroyCommandPool(m_device, m_commandPool, nullptr);

        vkDestroyRenderPass(m_device, m_renderPass, nullptr);

		vh::destroyFences(m_device, m_fences);

		vh::destroySemaphores(m_device, m_imageAvailableSemaphores, m_semaphores);

        vmaDestroyAllocator(m_vmaAllocator);

        vkDestroyDevice(m_device, nullptr);

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

		if (m_engine.GetDebug()) {
            vh::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }

        vkDestroyInstance(m_instance, nullptr);

    }


	//-------------------------------------------------------------------------------------------------------




	auto RendererVulkan::OnObjectCreate( Message message ) -> void {
		auto handle = message.template GetData<MsgObjectCreate>().m_handle;
		auto [gHandle, tHandle] = m_registry.template Get<GeometryHandle, TextureHandle>(handle);

		decltype(auto) geometry = m_registry.template Get<vh::Geometry&>(gHandle);
		if( geometry.m_vertexBuffer == VK_NULL_HANDLE ) {
			vh::createVertexBuffer(m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, geometry);
			vh::createIndexBuffer( m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, geometry);
		}

		decltype(auto) texture = m_registry.template Get<vh::Texture&>(tHandle);
		if( texture.m_textureImage == VK_NULL_HANDLE && texture.m_pixels != nullptr ) {
			vh::createTextureImage(m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, texture.m_pixels, texture.m_width, texture.m_height, texture.m_size, texture);
			vh::createTextureImageView(m_device, texture);
			vh::createTextureSampler(m_physicalDevice, m_device, texture);
		}

		vh::UniformBuffers ubo;
		vh::createUniformBuffers(m_physicalDevice, m_device, m_vmaAllocator, ubo);

		vh::DescriptorSets descriptorSets;
		vh::createDescriptorSets(m_device, texture, m_descriptorSetLayouts, ubo, m_descriptorPool, descriptorSets);
	    vh::updateDescriptorSets(m_device, texture, m_descriptorSetLayouts, ubo, m_descriptorPool, descriptorSets);

		m_registry.template Put(handle, ubo, descriptorSets);

		assert( m_registry.template Has<vh::UniformBuffers>(handle) );
		assert( m_registry.template Has<vh::DescriptorSets>(handle) );
	}
	

	auto RendererVulkan::OnTextureCreate( Message message ) -> void {
		auto msg = message.template GetData<MsgTextureCreate>();
		auto pixels = msg.m_pixels;
		auto handle = msg.m_handle;
		auto& texture = m_registry.template Get<vh::Texture&>(handle);
		vh::createTextureImage(m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, pixels, texture.m_width, texture.m_height, texture.m_size, texture);
		vh::createTextureImageView(m_device, texture);
		vh::createTextureSampler(m_physicalDevice, m_device, texture);
	}

	auto RendererVulkan::OnTextureDestroy( Message message ) -> void {
		auto handle = message.template GetData<MsgTextureDestroy>().m_handle;
		auto& texture = m_registry.template Get<vh::Texture&>(handle);
		vkDestroySampler(m_device, texture.m_textureSampler, nullptr);
		vkDestroyImageView(m_device, texture.m_textureImageView, nullptr);
		vh::destroyImage(m_device, m_vmaAllocator, texture.m_textureImage, texture.m_textureImageAllocation);
		m_registry.template Erase(handle);
	}

	auto RendererVulkan::OnGeometryCreate( Message message ) -> void {
		auto handle = message.template GetData<MsgGeometryCreate>().m_handle;
		auto& geometry = m_registry.template Get<vh::Geometry&>(handle);
		vh::createVertexBuffer(m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, geometry);
		vh::createIndexBuffer( m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, geometry);
	}

	auto RendererVulkan::OnGeometryDestroy( Message message ) -> void {
		auto handle = message.template GetData<MsgGeometryDestroy>().m_handle;
		auto& geometry = m_registry.template Get<vh::Geometry&>(handle);
		vh::destroyBuffer(m_device, m_vmaAllocator, geometry.m_indexBuffer, geometry.m_indexBufferAllocation);
		vh::destroyBuffer(m_device, m_vmaAllocator, geometry.m_vertexBuffer, geometry.m_vertexBufferAllocation);
		m_registry.template Erase(handle);
	}



};   // namespace vve