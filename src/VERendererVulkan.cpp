#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// Vulkan Renderer

    RendererVulkan::RendererVulkan(std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName) {

        engine.RegisterCallback( { 
			{this,      0, "EXTENSIONS", [this](Message& message){ return OnExtensions(message);} },
			{this,   1000, "INIT", [this](Message& message){ return OnInit(message);} }, 
			{this,      0, "PREPARE_NEXT_FRAME", [this](Message& message){ return OnPrepareNextFrame(message);} },
			{this,      0, "RECORD_NEXT_FRAME", [this](Message& message){ return OnRecordNextFrame(message);} },
			{this,      0, "RENDER_NEXT_FRAME", [this](Message& message){ return OnRenderNextFrame(message);} },

			{this,      0, "TEXTURE_CREATE",   [this](Message& message){ return OnTextureCreate(message);} },
			{this,      0, "TEXTURE_DESTROY",  [this](Message& message){ return OnTextureDestroy(message);} },
			{this,      0, "GEOMETRY_CREATE",  [this](Message& message){ return OnGeometryCreate(message);} },
			{this,      0, "GEOMETRY_DESTROY", [this](Message& message){ return OnGeometryDestroy(message);} },
			{this,   2000, "QUIT", [this](Message& message){ return OnQuit(message);} },
		} );
    }

    RendererVulkan::~RendererVulkan() {}

    bool RendererVulkan::OnExtensions(Message message) {
		auto msg = message.template GetData<MsgExtensions>();
		m_instanceExtensions.insert(m_instanceExtensions.end(), msg.m_instExt.begin(), msg.m_instExt.end());
		m_deviceExtensions.insert(m_deviceExtensions.end(), msg.m_devExt.begin(), msg.m_devExt.end());
		return false;
	}

    bool RendererVulkan::OnInit(Message message) {
    	GetVulkanState().m_windowSDL = (WindowSDL*)m_window;
		if (m_engine.GetDebug()) {
            m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

    	vh::createInstance( m_validationLayers, m_instanceExtensions, m_engine.GetDebug(), GetVulkanState().m_instance);
		if (m_engine.GetDebug()) {
	        vh::setupDebugMessenger(GetInstance(), GetVulkanState().m_debugMessenger);
		}

		if (SDL_Vulkan_CreateSurface(GetVulkanState().m_windowSDL->GetSDLWindow(), GetInstance(), &GetVulkanState().m_surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }

        vh::pickPhysicalDevice(GetInstance(), m_deviceExtensions, GetSurface(), GetVulkanState().m_physicalDevice);
        vh::createLogicalDevice(GetSurface(), GetPhysicalDevice(), GetVulkanState().m_queueFamilies, m_validationLayers, 
			m_deviceExtensions, m_engine.GetDebug(), GetVulkanState().m_device, GetVulkanState().m_graphicsQueue, GetVulkanState().m_presentQueue);
        vh::initVMA(GetInstance(), GetPhysicalDevice(), GetDevice(), GetVulkanState().m_vmaAllocator);  
        vh::createSwapChain(GetVulkanState().m_windowSDL->GetSDLWindow(), GetSurface(), GetPhysicalDevice(), GetDevice(), GetVulkanState().m_swapChain);
        vh::createImageViews(GetDevice(), GetSwapChain());
        vh::createRenderPassClear(GetPhysicalDevice(), GetDevice(), GetSwapChain(), true, m_renderPass);

		vh::createDescriptorSetLayout( GetDevice(), {}, m_descriptorSetLayoutPerFrame );
			
        vh::createGraphicsPipeline(GetDevice(), m_renderPass, "shaders\\Vulkan\\vert.spv", "", 
			 { m_descriptorSetLayoutPerFrame }, m_graphicsPipeline);

        vh::createCommandPool(GetSurface(), GetPhysicalDevice(), GetDevice(), m_commandPool);
        vh::createDepthResources(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage());
        vh::createFramebuffers(GetDevice(), GetSwapChain(), GetDepthImage(), m_renderPass);

        vh::createCommandBuffers(GetDevice(), m_commandPool, m_commandBuffers);
        vh::createDescriptorPool(GetDevice(), 1000, m_descriptorPool);
        vh::createSemaphores(GetDevice(), 3, m_imageAvailableSemaphores, m_semaphores);
		vh::createFences(GetDevice(), MAX_FRAMES_IN_FLIGHT, m_fences);
		return false;
    }

    bool RendererVulkan::OnPrepareNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return false;

        GetCurrentFrame() = (GetCurrentFrame() + 1) % MAX_FRAMES_IN_FLIGHT;
		GetVulkanState().m_commandBuffersSubmit.clear();

		vkWaitForFences(GetDevice(), 1, &m_fences[GetCurrentFrame()], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(GetDevice(), GetSwapChain().m_swapChain, UINT64_MAX,
                            m_imageAvailableSemaphores[GetCurrentFrame()], VK_NULL_HANDLE, &GetImageIndex());

        if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
            recreateSwapChain(GetVulkanState().m_windowSDL->GetSDLWindow(), GetSurface(), GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage(), m_renderPass);
        } else assert (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
		return false;
    }

    bool RendererVulkan::OnRecordNextFrame(Message message) {
		if(GetVulkanState().m_windowSDL->GetIsMinimized()) return false;

        vkResetCommandBuffer(m_commandBuffers[GetCurrentFrame()],  0);

		vh::startRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
			GetSwapChain(), m_renderPass, m_graphicsPipeline, 
			true, ((WindowSDL*)m_window)->GetClearColor(), GetCurrentFrame());

		vh::endRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()]);

		SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
		return false;
	}

    bool RendererVulkan::OnRenderNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return false;
        
		VkSemaphore signalSemaphore;
		vh::submitCommandBuffers(GetDevice(), GetGraphicsQueue(), GetVulkanState().m_commandBuffersSubmit, 
			m_imageAvailableSemaphores, m_semaphores, signalSemaphore, m_fences, GetCurrentFrame());
				
   		vh::transitionImageLayout(GetDevice(), GetGraphicsQueue(), m_commandPool, 
			GetSwapChain().m_swapChainImages[GetImageIndex()], GetSwapChain().m_swapChainImageFormat, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		VkResult result = vh::presentImage(GetPresentQueue(), GetSwapChain(), GetImageIndex(), signalSemaphore);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || GetFramebufferResized()) {
            GetFramebufferResized() = false;
            vh::recreateSwapChain(GetVulkanState().m_windowSDL->GetSDLWindow(), GetSurface(), GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage(), m_renderPass);
        } else assert(result == VK_SUCCESS);
		return false;
    }
    
    bool RendererVulkan::OnQuit(Message message) {

        vkDeviceWaitIdle(GetDevice());

        vh::cleanupSwapChain(GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage());

        vkDestroyPipeline(GetDevice(), m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(GetDevice(), m_graphicsPipeline.m_pipelineLayout, nullptr);

        vkDestroyDescriptorPool(GetDevice(), m_descriptorPool, nullptr);

		for( decltype(auto) texture : m_registry.template GetView<vh::Texture&>() ) {
			vkDestroySampler(GetDevice(), texture.m_textureSampler, nullptr);
        	vkDestroyImageView(GetDevice(), texture.m_textureImageView, nullptr);
	        vh::destroyImage(GetDevice(), GetVmaAllocator(), texture.m_textureImage, texture.m_textureImageAllocation);
		}

		vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayoutPerFrame, nullptr);

		for( auto geometry : m_registry.template GetView<vh::Mesh&>() ) {
	        vh::destroyBuffer(GetDevice(), GetVmaAllocator(), geometry.m_indexBuffer, geometry.m_indexBufferAllocation);
	        vh::destroyBuffer(GetDevice(), GetVmaAllocator(), geometry.m_vertexBuffer, geometry.m_vertexBufferAllocation);
		}

		for( auto ubo : m_registry.template GetView<vh::UniformBuffers&>() ) {
			vh::destroyBuffer2(GetDevice(), GetVmaAllocator(), ubo);
		}

        vkDestroyCommandPool(GetDevice(), m_commandPool, nullptr);

        vkDestroyRenderPass(GetDevice(), m_renderPass, nullptr);

		vh::destroyFences(GetDevice(), m_fences);

		vh::destroySemaphores(GetDevice(), m_imageAvailableSemaphores, m_semaphores);

        vmaDestroyAllocator(GetVmaAllocator());

        vkDestroyDevice(GetDevice(), nullptr);

        vkDestroySurfaceKHR(GetInstance(), GetSurface(), nullptr);

		if (m_engine.GetDebug()) {
            vh::DestroyDebugUtilsMessengerEXT(GetInstance(), GetVulkanState().m_debugMessenger, nullptr);
        }

        vkDestroyInstance(GetInstance(), nullptr);
		return false;
    }


	//-------------------------------------------------------------------------------------------------------


	bool RendererVulkan::OnTextureCreate( Message message ) {
		auto msg = message.template GetData<MsgTextureCreate>();
		auto pixels = msg.m_pixels;
		auto handle = msg.m_handle;
		auto& texture = m_registry.template Get<vh::Texture&>(handle);
		vh::createTextureImage(GetPhysicalDevice(), GetDevice(), GetVulkanState().m_vmaAllocator, GetGraphicsQueue(), m_commandPool, pixels, texture.m_width, texture.m_height, texture.m_size, texture);
		vh::createTextureImageView(GetDevice(), texture);
		vh::createTextureSampler(GetPhysicalDevice(), GetDevice(), texture);
		return false;
	}

	bool RendererVulkan::OnTextureDestroy( Message message ) {
		auto handle = message.template GetData<MsgTextureDestroy>().m_handle;
		auto& texture = m_registry.template Get<vh::Texture&>(handle);
		vkDestroySampler(GetDevice(), texture.m_textureSampler, nullptr);
		vkDestroyImageView(GetDevice(), texture.m_textureImageView, nullptr);
		vh::destroyImage(GetDevice(), GetVulkanState().m_vmaAllocator, texture.m_textureImage, texture.m_textureImageAllocation);
		m_registry.Erase(handle);
		return false;
	}

	bool RendererVulkan::OnGeometryCreate( Message message ) {
		auto handle = message.template GetData<MsgGeometryCreate>().m_handle;
		auto& geometry = m_registry.template Get<vh::Mesh&>(handle);
		vh::createVertexBuffer2(GetPhysicalDevice(), GetDevice(), GetVulkanState().m_vmaAllocator, GetGraphicsQueue(), m_commandPool, geometry);
		vh::createIndexBuffer( GetPhysicalDevice(), GetDevice(), GetVulkanState().m_vmaAllocator, GetGraphicsQueue(), m_commandPool, geometry);
		return false;
	}

	bool RendererVulkan::OnGeometryDestroy( Message message ) {
		auto handle = message.template GetData<MsgGeometryDestroy>().m_handle;
		auto& geometry = m_registry.template Get<vh::Mesh&>(handle);
		vh::destroyBuffer(GetDevice(), GetVulkanState().m_vmaAllocator, geometry.m_indexBuffer, geometry.m_indexBufferAllocation);
		vh::destroyBuffer(GetDevice(), GetVulkanState().m_vmaAllocator, geometry.m_vertexBuffer, geometry.m_vertexBufferAllocation);
		m_registry.Erase(handle);
		return false;
	}



};   // namespace vve