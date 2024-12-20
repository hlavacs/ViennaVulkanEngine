
#include "VHInclude.h"
#include "VHVulkan.h"

#include "VERendererVulkan.h"
#include "VEEngine.h"
#include "VEWindowSDL.h"
#include "VESceneManager.h"



namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// Vulkan Renderer

    template<ArchitectureType ATYPE>
    RendererVulkan<ATYPE>::RendererVulkan(std::string systemName, Engine<ATYPE>* engine, Window<ATYPE>* window ) 
        : Renderer<ATYPE>(systemName, engine, window) {

        engine->RegisterCallback( { 
			{this,  -2000, "INIT", [this](Message message){this->OnInit(message);} }, 
			{this,   1000, "INIT", [this](Message message){this->OnInit2(message);} },
			{this,      0, "PREPARE_NEXT_FRAME", [this](Message message){this->OnPrepareNextFrame(message);} },
			{this,      0, "RECORD_NEXT_FRAME", [this](Message message){this->OnRecordNextFrame(message);} },
			{this,      0, "RENDER_NEXT_FRAME", [this](Message message){this->OnRenderNextFrame(message);} },
			{this,      0, "TEXTURE_CREATE",   [this](Message message){this->OnTextureCreate(message);} },
			{this,      0, "TEXTURE_DESTROY",  [this](Message message){this->OnTextureDestroy(message);} },
			{this,      0, "GEOMETRY_CREATE",  [this](Message message){this->OnTextureCreate(message);} },
			{this,      0, "GEOMETRY_DESTROY", [this](Message message){this->OnTextureDestroy(message);} },
			{this,  -1000, "QUIT", [this](Message message){this->OnQuit(message);} },
			{this,   1000, "QUIT", [this](Message message){this->OnQuit2(message);} }
		} );
    }

    template<ArchitectureType ATYPE>
    RendererVulkan<ATYPE>::~RendererVulkan() {}


    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnInit(Message message) {
    	m_windowSDL = (WindowSDL<ATYPE>*)m_window;
		auto instanceExtensions = m_windowSDL->GetInstanceExtensions();
		if (m_engine->GetDebug()) {
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
    	vh::createInstance( m_validationLayers, instanceExtensions, m_instance);
		if (m_engine->GetDebug()) {
	        vh::setupDebugMessenger(m_instance, m_debugMessenger);
		}
    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnInit2(Message message) {
        vh::pickPhysicalDevice(m_instance, m_deviceExtensions, m_window->GetSurface(), m_physicalDevice);
        vh::createLogicalDevice(m_window->GetSurface(), m_physicalDevice, m_queueFamilies, m_validationLayers, m_deviceExtensions, m_device, m_graphicsQueue, m_presentQueue);
        vh::initVMA(m_instance, m_physicalDevice, m_device, m_vmaAllocator);  
        vh::createSwapChain(m_windowSDL->GetSDLWindow(), m_window->GetSurface(), m_physicalDevice, m_device, m_swapChain);
        vh::createImageViews(m_device, m_swapChain);
        vh::createRenderPassClear(m_physicalDevice, m_device, m_swapChain, true, m_renderPass);
        vh::createDescriptorSetLayout(m_device, m_descriptorSetLayout);
        vh::createGraphicsPipeline(m_device, m_renderPass, m_descriptorSetLayout, m_graphicsPipeline);
        vh::createCommandPool(m_window->GetSurface(), m_physicalDevice, m_device, m_commandPool);
        vh::createDepthResources(m_physicalDevice, m_device, m_vmaAllocator, m_swapChain, m_depthImage);
        vh::createFramebuffers(m_device, m_swapChain, m_depthImage, m_renderPass);

        vh::createTextureImage(m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, "assets\\textures\\viking_room.png", m_texture);
        vh::createTextureImageView(m_device, m_texture);
        vh::createTextureSampler(m_physicalDevice, m_device, m_texture);

		vh::loadModel("assets\\models\\viking_room.obj", m_geometry);
        vh::createVertexBuffer(m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, m_geometry);
        vh::createIndexBuffer( m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, m_geometry);

        vh::createCommandBuffers(m_device, m_commandPool, m_commandBuffers);
        vh::createUniformBuffers(m_physicalDevice, m_device, m_vmaAllocator, m_uniformBuffers);
        vh::createDescriptorPool(m_device, 1000, m_descriptorPool);
        vh::createDescriptorSets(m_device, m_texture, m_descriptorSetLayout, m_uniformBuffers, m_descriptorPool, m_descriptorSets);
        vh::createSemaphores(m_device, 1, m_imageAvailableSemaphores, m_semaphores);
		vh::createFences(m_device, MAX_FRAMES_IN_FLIGHT, m_fences);
    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnPrepareNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return;

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		m_commandBuffersSubmit.clear();

		vkWaitForFences(m_device, 1, &m_fences[m_currentFrame], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain.m_swapChain, UINT64_MAX,
                            m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &m_imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
            recreateSwapChain(m_windowSDL->GetSDLWindow(), m_window->GetSurface(), m_physicalDevice, m_device, m_vmaAllocator, m_swapChain, m_depthImage, m_renderPass);
            return;
        } else assert (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
    }


    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnRecordNextFrame(Message message) {
		if(m_windowSDL->GetIsMinimized()) return;

        vkResetCommandBuffer(m_commandBuffers[m_currentFrame],  0);

		vh::startRecordCommandBuffer(m_commandBuffers[m_currentFrame], m_imageIndex, 
			m_swapChain, m_renderPass, m_graphicsPipeline, m_descriptorSets, 
			true, ((WindowSDL<ATYPE>*)m_window)->GetClearColor(), m_currentFrame);

		vh::endRecordCommandBuffer(m_commandBuffers[m_currentFrame]);

		SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
	}

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnRenderNextFrame(Message message) {
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
            recreateSwapChain(m_windowSDL->GetSDLWindow(), m_window->GetSurface(), m_physicalDevice, m_device, m_vmaAllocator, m_swapChain, m_depthImage, m_renderPass);
        } else assert(result == VK_SUCCESS);
    }
    
    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnQuit(Message message) {

        vkDeviceWaitIdle(m_device);

        vh::cleanupSwapChain(m_device, m_vmaAllocator, m_swapChain, m_depthImage);

        vkDestroyPipeline(m_device, m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_graphicsPipeline.m_pipelineLayout, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vh::destroyBuffer(m_device, m_vmaAllocator, m_uniformBuffers.m_uniformBuffers[i], m_uniformBuffers.m_uniformBuffersAllocation[i]);
        }

        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

        vkDestroySampler(m_device, m_texture.m_textureSampler, nullptr);
        vkDestroyImageView(m_device, m_texture.m_textureImageView, nullptr);

        vh::destroyImage(m_device, m_vmaAllocator, m_texture.m_textureImage, m_texture.m_textureImageAllocation);

        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

        vh::destroyBuffer(m_device, m_vmaAllocator, m_geometry.m_indexBuffer, m_geometry.m_indexBufferAllocation);

        vh::destroyBuffer(m_device, m_vmaAllocator, m_geometry.m_vertexBuffer, m_geometry.m_vertexBufferAllocation);

        vkDestroyCommandPool(m_device, m_commandPool, nullptr);

        vkDestroyRenderPass(m_device, m_renderPass, nullptr);

		vh::destroyFences(m_device, m_fences);

		vh::destroySemaphores(m_device, m_imageAvailableSemaphores, m_semaphores);

        vmaDestroyAllocator(m_vmaAllocator);

        vkDestroyDevice(m_device, nullptr);

		if (m_engine->GetDebug()) {
            vh::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }
    }

	template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnQuit2(Message message) {
        vkDestroyInstance(m_instance, nullptr);
	}

	template<ArchitectureType ATYPE>
	auto RendererVulkan<ATYPE>::OnTextureCreate( Message message ) -> void {
		auto pixels = message.GetData<MsgTextureCreate>().m_pixels;
		auto handle = message.GetData<MsgTextureCreate>().m_handle;
		auto& texture = m_engine->GetRegistry().template Get<vh::Texture&>(handle);
		vh::createTextureImage2(m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, 
			pixels, texture.m_width, texture.m_height, texture.m_size, texture);
		vh::createTextureImageView(m_device, texture);
		vh::createTextureSampler(m_physicalDevice, m_device, texture);
	}

	template<ArchitectureType ATYPE>
	auto RendererVulkan<ATYPE>::OnTextureDestroy( Message message ) -> void {
		auto handle = message.GetData<MsgTextureDestroy>().m_handle;
		auto& texture = m_engine->GetRegistry().template Get<vh::Texture&>(handle);
		vkDestroySampler(m_device, texture.m_textureSampler, nullptr);
		vkDestroyImageView(m_device, texture.m_textureImageView, nullptr);
		vh::destroyImage(m_device, m_vmaAllocator, texture.m_textureImage, texture.m_textureImageAllocation);
		m_engine->GetRegistry().template Erase(handle);
	}

	template<ArchitectureType ATYPE>
	auto RendererVulkan<ATYPE>::OnGeometryCreate( Message message ) -> void {
		auto handle = message.GetData<MsgGeometryCreate>().m_handle;
		auto& geometry = m_engine->GetRegistry().template Get<vh::Geometry&>(handle);
		vh::createVertexBuffer(m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, geometry);
		vh::createIndexBuffer( m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, geometry);
	}

	template<ArchitectureType ATYPE>
	auto RendererVulkan<ATYPE>::OnGeometryDestroy( Message message ) -> void {
		auto handle = message.GetData<MsgGeometryDestroy>().m_handle;
		auto& geometry = m_engine->GetRegistry().template Get<vh::Geometry&>(handle);
		vh::destroyBuffer(m_device, m_vmaAllocator, geometry.m_indexBuffer, geometry.m_indexBufferAllocation);
		vh::destroyBuffer(m_device, m_vmaAllocator, geometry.m_vertexBuffer, geometry.m_vertexBufferAllocation);
		m_engine->GetRegistry().template Erase(handle);
	}


    template class RendererVulkan<ENGINETYPE_SEQUENTIAL>;
    template class RendererVulkan<ENGINETYPE_PARALLEL>;

};   // namespace vve