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

			{this,   1000, "TEXTURE_CREATE",   [this](Message& message){ return OnTextureCreate(message);} },
			{this,      0, "TEXTURE_DESTROY",  [this](Message& message){ return OnTextureDestroy(message);} },
			{this,      0, "MESH_CREATE",  [this](Message& message){ return OnMeshCreate(message);} },
			{this,      0, "MESH_DESTROY", [this](Message& message){ return OnMeshDestroy(message);} },
			{this,   2000, "QUIT", [this](Message& message){ return OnQuit(message);} },
		} );

		m_vulkanStateHandle = m_registry.Insert(VulkanState{});
		GetState2();
    }

    RendererVulkan::~RendererVulkan() {}

    bool RendererVulkan::OnExtensions(Message message) {
		auto msg = message.template GetData<MsgExtensions>();
		m_instanceExtensions.insert(m_instanceExtensions.end(), msg.m_instExt.begin(), msg.m_instExt.end());
		m_deviceExtensions.insert(m_deviceExtensions.end(), msg.m_devExt.begin(), msg.m_devExt.end());
		return false;
	}

    bool RendererVulkan::OnInit(Message message) {
		if (m_engine.GetDebug()) {
            m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

    	vh::DevCreateInstance( m_validationLayers, m_instanceExtensions, m_engine.GetDebug(), m_vulkanState().m_instance);
		if (m_engine.GetDebug()) {
	        vh::DevSetupDebugMessenger(m_vulkanState().m_instance, m_vulkanState().m_debugMessenger);
		}

		if (SDL_Vulkan_CreateSurface(m_windowSDLState().m_sdlWindow, m_vulkanState().m_instance, &m_vulkanState().m_surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }

        vh::DevPickPhysicalDevice(m_vulkanState().m_instance, m_deviceExtensions, m_vulkanState().m_surface, m_vulkanState().m_physicalDevice);
        vh::DevCreateLogicalDevice(m_vulkanState().m_surface, m_vulkanState().m_physicalDevice, m_vulkanState().m_queueFamilies, m_validationLayers, 
			m_deviceExtensions, m_engine.GetDebug(), m_vulkanState().m_device, m_vulkanState().m_graphicsQueue, m_vulkanState().m_presentQueue);
        vh::DevInitVMA(m_vulkanState().m_instance, m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator);  
        vh::DevCreateSwapChain(m_windowSDLState().m_sdlWindow, 
			m_vulkanState().m_surface, m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_swapChain);
        
		
		vh::DevCreateImageViews(m_vulkanState().m_device, m_vulkanState().m_swapChain);
        vh::RenCreateRenderPassClear(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_swapChain, true, m_renderPass);

		vh::RenCreateDescriptorSetLayout( m_vulkanState().m_device, {}, m_descriptorSetLayoutPerFrame );
			
        vh::RenCreateGraphicsPipeline(m_vulkanState().m_device, m_renderPass, "shaders\\Vulkan\\vert.spv", "", {}, {},
			 { m_descriptorSetLayoutPerFrame }, {}, m_graphicsPipeline);

        vh::ComCreateCommandPool(m_vulkanState().m_surface, m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_commandPool);
        vh::RenCreateDepthResources(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_vulkanState().m_swapChain, m_vulkanState().m_depthImage);
        vh::RenCreateFramebuffers(m_vulkanState().m_device, m_vulkanState().m_swapChain, m_vulkanState().m_depthImage, m_renderPass);

        vh::ComCreateCommandBuffers(m_vulkanState().m_device, m_commandPool, m_commandBuffers);
        vh::RenCreateDescriptorPool(m_vulkanState().m_device, 1000, m_descriptorPool);
        vh::SynCreateSemaphores(m_vulkanState().m_device, 3, m_imageAvailableSemaphores, m_semaphores);
		vh::SynCreateFences(m_vulkanState().m_device, MAX_FRAMES_IN_FLIGHT, m_fences);
		return false;
    }

    bool RendererVulkan::OnPrepareNextFrame(Message message) {
        if(m_windowState().m_isMinimized) return false;

        m_vulkanState().m_currentFrame = (m_vulkanState().m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		m_vulkanState().m_commandBuffersSubmit.clear();

		vkWaitForFences(m_vulkanState().m_device, 1, &m_fences[m_vulkanState().m_currentFrame], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(m_vulkanState().m_device, m_vulkanState().m_swapChain.m_swapChain, UINT64_MAX,
                            m_imageAvailableSemaphores[m_vulkanState().m_currentFrame], VK_NULL_HANDLE, &m_vulkanState().m_imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
			auto m_windowSDLState = WindowSDL::GetState(m_registry);
            DevRecreateSwapChain( std::get<2>(m_windowSDLState)().m_sdlWindow, 
				m_vulkanState().m_surface, m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, 
				m_vulkanState().m_swapChain, m_vulkanState().m_depthImage, m_renderPass);

			m_engine.SendMessage( MsgWindowSize{this, nullptr} );
        } else assert (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
		return false;
    }

    bool RendererVulkan::OnRecordNextFrame(Message message) {
		if(m_windowState().m_isMinimized) return false;

        vkResetCommandBuffer(m_commandBuffers[m_vulkanState().m_currentFrame],  0);

		vh::ComStartRecordCommandBuffer(m_commandBuffers[m_vulkanState().m_currentFrame], m_vulkanState().m_imageIndex, 
			m_vulkanState().m_swapChain, m_renderPass, m_graphicsPipeline, true, 
			std::get<1>(Window::GetState(m_registry, m_windowName))().m_clearColor, 
			m_vulkanState().m_currentFrame);

		vh::ComEndRecordCommandBuffer(m_commandBuffers[m_vulkanState().m_currentFrame]);

		SubmitCommandBuffer(m_commandBuffers[m_vulkanState().m_currentFrame]);
		return false;
	}

    bool RendererVulkan::OnRenderNextFrame(Message message) {
        if(m_windowState().m_isMinimized) return false;
        
		VkSemaphore signalSemaphore;
		vh::ComSubmitCommandBuffers(m_vulkanState().m_device, m_vulkanState().m_graphicsQueue, m_vulkanState().m_commandBuffersSubmit, 
			m_imageAvailableSemaphores, m_semaphores, signalSemaphore, m_fences, m_vulkanState().m_currentFrame);
				
   		vh::ImgTransitionImageLayout(m_vulkanState().m_device, m_vulkanState().m_graphicsQueue, m_commandPool, 
			m_vulkanState().m_swapChain.m_swapChainImages[m_vulkanState().m_imageIndex], m_vulkanState().m_swapChain.m_swapChainImageFormat, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		VkResult result = vh::ComPresentImage(m_vulkanState().m_presentQueue, m_vulkanState().m_swapChain, m_vulkanState().m_imageIndex, signalSemaphore);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_vulkanState().m_framebufferResized) {
            m_vulkanState().m_framebufferResized = false;
			auto m_windowSDLState = WindowSDL::GetState(m_registry);
            vh::DevRecreateSwapChain(std::get<2>(m_windowSDLState)().m_sdlWindow, 
				m_vulkanState().m_surface, m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, 
				m_vulkanState().m_swapChain, m_vulkanState().m_depthImage, m_renderPass);

			m_engine.SendMessage( MsgWindowSize{this, nullptr} );
        } else assert(result == VK_SUCCESS);
		return false;
    }
    
    bool RendererVulkan::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vulkanState().m_device);

        vh::DevCleanupSwapChain(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_vulkanState().m_swapChain, m_vulkanState().m_depthImage);

        vkDestroyPipeline(m_vulkanState().m_device, m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_vulkanState().m_device, m_graphicsPipeline.m_pipelineLayout, nullptr);

        vkDestroyDescriptorPool(m_vulkanState().m_device, m_descriptorPool, nullptr);

		for( decltype(auto) texture : m_registry.template GetView<vh::Map&>() ) {
			vkDestroySampler(m_vulkanState().m_device, texture().m_mapSampler, nullptr);
        	vkDestroyImageView(m_vulkanState().m_device, texture().m_mapImageView, nullptr);
	        vh::ImgDestroyImage(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, texture().m_mapImage, texture().m_mapImageAllocation);
		}

		vkDestroyDescriptorSetLayout(m_vulkanState().m_device, m_descriptorSetLayoutPerFrame, nullptr);

		for( auto geometry : m_registry.template GetView<vh::Mesh&>() ) {
	        vh::BufDestroyBuffer(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, geometry().m_indexBuffer, geometry().m_indexBufferAllocation);
	        vh::BufDestroyBuffer(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, geometry().m_vertexBuffer, geometry().m_vertexBufferAllocation);
		}

		for( auto ubo : m_registry.template GetView<vh::UniformBuffers&>() ) {
			vh::BufDestroyBuffer2(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, ubo);
		}

        vkDestroyCommandPool(m_vulkanState().m_device, m_commandPool, nullptr);

        vkDestroyRenderPass(m_vulkanState().m_device, m_renderPass, nullptr);

		vh::SynDestroyFences(m_vulkanState().m_device, m_fences);

		vh::SynDestroySemaphores(m_vulkanState().m_device, m_imageAvailableSemaphores, m_semaphores);

        vmaDestroyAllocator(m_vulkanState().m_vmaAllocator);

        vkDestroyDevice(m_vulkanState().m_device, nullptr);

        vkDestroySurfaceKHR(m_vulkanState().m_instance, m_vulkanState().m_surface, nullptr);

		if (m_engine.GetDebug()) {
            vh::DevDestroyDebugUtilsMessengerEXT(m_vulkanState().m_instance, m_vulkanState().m_debugMessenger, nullptr);
        }

        vkDestroyInstance(m_vulkanState().m_instance, nullptr);
		return false;
    }


	//-------------------------------------------------------------------------------------------------------


	bool RendererVulkan::OnTextureCreate( Message message ) {
		auto m_vulkanState = GetState2();

		auto msg = message.template GetData<MsgTextureCreate>();
		auto handle = msg.m_handle;
		auto texture = m_registry.template Get<vh::Map&>(handle);
		auto pixels = texture().m_pixels;

		vh::ImgCreateTextureImage(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_vulkanState().m_graphicsQueue, m_commandPool, pixels, texture().m_width, texture().m_height, texture().m_size, texture);
		vh::ImgCreateTextureImageView(m_vulkanState().m_device, texture);
		vh::ImgCreateTextureSampler(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, texture);
		return false;
	}

	bool RendererVulkan::OnTextureDestroy( Message message ) {
		auto m_vulkanState = GetState2();

		auto handle = message.template GetData<MsgTextureDestroy>().m_handle;
		auto texture = m_registry.template Get<vh::Map&>(handle);
		vkDestroySampler(m_vulkanState().m_device, texture().m_mapSampler, nullptr);
		vkDestroyImageView(m_vulkanState().m_device, texture().m_mapImageView, nullptr);
		vh::ImgDestroyImage(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, texture().m_mapImage, texture().m_mapImageAllocation);
		m_registry.Erase(handle);
		return false;
	}

	bool RendererVulkan::OnMeshCreate( Message message ) {
		auto m_vulkanState = GetState2();

		auto handle = message.template GetData<MsgMeshCreate>().m_handle;
		auto mesh = m_registry.template Get<vh::Mesh&>(handle);
		vh::BufCreateVertexBuffer(m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_vulkanState().m_graphicsQueue, m_commandPool, mesh);
		vh::BufCreateIndexBuffer( m_vulkanState().m_physicalDevice, m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, m_vulkanState().m_graphicsQueue, m_commandPool, mesh);
		return false;
	}

	bool RendererVulkan::OnMeshDestroy( Message message ) {
		auto m_vulkanState = GetState2();

		auto handle = message.template GetData<MsgMeshDestroy>().m_handle;
		auto mesh = m_registry.template Get<vh::Mesh&>(handle);
		vh::BufDestroyBuffer(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, mesh().m_indexBuffer, mesh().m_indexBufferAllocation);
		vh::BufDestroyBuffer(m_vulkanState().m_device, m_vulkanState().m_vmaAllocator, mesh().m_vertexBuffer, mesh().m_vertexBufferAllocation);
		m_registry.Erase(handle);
		return false;
	}



};   // namespace vve