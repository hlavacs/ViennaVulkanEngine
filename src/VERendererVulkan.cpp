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
    }

    RendererVulkan::~RendererVulkan() {}

    bool RendererVulkan::OnExtensions(Message message) {
		auto msg = message.template GetData<MsgExtensions>();
		m_instanceExtensions.insert(m_instanceExtensions.end(), msg.m_instExt.begin(), msg.m_instExt.end());
		m_deviceExtensions.insert(m_deviceExtensions.end(), msg.m_devExt.begin(), msg.m_devExt.end());
		return false;
	}

    bool RendererVulkan::OnInit(Message message) {
		GetVulkanState()().m_windowSDL = (WindowSDL*)m_window;
		if (m_engine.GetDebug()) {
            m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

    	vh::DevCreateInstance( m_validationLayers, m_instanceExtensions, m_engine.GetDebug(), GetVulkanState()().m_instance);
		if (m_engine.GetDebug()) {
	        vh::DevSetupDebugMessenger(GetInstance(), GetVulkanState()().m_debugMessenger);
		}

		if (SDL_Vulkan_CreateSurface(GetVulkanState()().m_windowSDL->GetSDLWindow(), GetInstance(), &GetVulkanState()().m_surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }

        vh::DevPickPhysicalDevice(GetInstance(), m_deviceExtensions, GetSurface(), GetVulkanState()().m_physicalDevice);
        vh::DevCreateLogicalDevice(GetSurface(), GetPhysicalDevice(), GetVulkanState()().m_queueFamilies, m_validationLayers, 
			m_deviceExtensions, m_engine.GetDebug(), GetVulkanState()().m_device, GetVulkanState()().m_graphicsQueue, GetVulkanState()().m_presentQueue);
        vh::DevInitVMA(GetInstance(), GetPhysicalDevice(), GetDevice(), GetVulkanState()().m_vmaAllocator);  
        vh::DevCreateSwapChain(GetVulkanState()().m_windowSDL->GetSDLWindow(), GetSurface(), GetPhysicalDevice(), GetDevice(), GetVulkanState()().m_swapChain);
        vh::DevCreateImageViews(GetDevice(), GetSwapChain());
        vh::RenCreateRenderPassClear(GetPhysicalDevice(), GetDevice(), GetSwapChain(), true, m_renderPass);

		vh::RenCreateDescriptorSetLayout( GetDevice(), {}, m_descriptorSetLayoutPerFrame );
			
        vh::RenCreateGraphicsPipeline(GetDevice(), m_renderPass, "shaders\\Vulkan\\vert.spv", "", {}, {},
			 { m_descriptorSetLayoutPerFrame }, {}, m_graphicsPipeline);

        vh::ComCreateCommandPool(GetSurface(), GetPhysicalDevice(), GetDevice(), m_commandPool);
        vh::RenCreateDepthResources(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage());
        vh::RenCreateFramebuffers(GetDevice(), GetSwapChain(), GetDepthImage(), m_renderPass);

        vh::ComCreateCommandBuffers(GetDevice(), m_commandPool, m_commandBuffers);
        vh::RenCreateDescriptorPool(GetDevice(), 1000, m_descriptorPool);
        vh::SynCreateSemaphores(GetDevice(), 3, m_imageAvailableSemaphores, m_semaphores);
		vh::SynCreateFences(GetDevice(), MAX_FRAMES_IN_FLIGHT, m_fences);
		return false;
    }

    bool RendererVulkan::OnPrepareNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return false;

        GetCurrentFrame() = (GetCurrentFrame() + 1) % MAX_FRAMES_IN_FLIGHT;
		GetVulkanState()().m_commandBuffersSubmit.clear();

		vkWaitForFences(GetDevice(), 1, &m_fences[GetCurrentFrame()], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(GetDevice(), GetSwapChain().m_swapChain, UINT64_MAX,
                            m_imageAvailableSemaphores[GetCurrentFrame()], VK_NULL_HANDLE, &GetImageIndex());

        if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
            DevRecreateSwapChain(GetVulkanState()().m_windowSDL->GetSDLWindow(), GetSurface(), GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage(), m_renderPass);
			m_engine.SendMessage( MsgWindowSize{this, nullptr} );
        } else assert (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
		return false;
    }

    bool RendererVulkan::OnRecordNextFrame(Message message) {
		if(GetVulkanState()().m_windowSDL->GetIsMinimized()) return false;

        vkResetCommandBuffer(m_commandBuffers[GetCurrentFrame()],  0);

		vh::ComStartRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
			GetSwapChain(), m_renderPass, m_graphicsPipeline, 
			true, 
			std::get<1>(GetWindowState(m_registry))().m_clearColor, //((WindowSDL*)m_window)->GetClearColor(), 
			GetCurrentFrame());

		vh::ComEndRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()]);

		SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
		return false;
	}

    bool RendererVulkan::OnRenderNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return false;
        
		VkSemaphore signalSemaphore;
		vh::ComSubmitCommandBuffers(GetDevice(), GetGraphicsQueue(), GetVulkanState()().m_commandBuffersSubmit, 
			m_imageAvailableSemaphores, m_semaphores, signalSemaphore, m_fences, GetCurrentFrame());
				
   		vh::ImgTransitionImageLayout(GetDevice(), GetGraphicsQueue(), m_commandPool, 
			GetSwapChain().m_swapChainImages[GetImageIndex()], GetSwapChain().m_swapChainImageFormat, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		VkResult result = vh::ComPresentImage(GetPresentQueue(), GetSwapChain(), GetImageIndex(), signalSemaphore);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || GetFramebufferResized()) {
            GetFramebufferResized() = false;
            vh::DevRecreateSwapChain(GetVulkanState()().m_windowSDL->GetSDLWindow(), GetSurface(), GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage(), m_renderPass);
			m_engine.SendMessage( MsgWindowSize{this, nullptr} );
        } else assert(result == VK_SUCCESS);
		return false;
    }
    
    bool RendererVulkan::OnQuit(Message message) {

        vkDeviceWaitIdle(GetDevice());

        vh::DevCleanupSwapChain(GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage());

        vkDestroyPipeline(GetDevice(), m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(GetDevice(), m_graphicsPipeline.m_pipelineLayout, nullptr);

        vkDestroyDescriptorPool(GetDevice(), m_descriptorPool, nullptr);

		for( decltype(auto) texture : m_registry.template GetView<vh::Map&>() ) {
			vkDestroySampler(GetDevice(), texture().m_mapSampler, nullptr);
        	vkDestroyImageView(GetDevice(), texture().m_mapImageView, nullptr);
	        vh::ImgDestroyImage(GetDevice(), GetVmaAllocator(), texture().m_mapImage, texture().m_mapImageAllocation);
		}

		vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayoutPerFrame, nullptr);

		for( auto geometry : m_registry.template GetView<vh::Mesh&>() ) {
	        vh::BufDestroyBuffer(GetDevice(), GetVmaAllocator(), geometry().m_indexBuffer, geometry().m_indexBufferAllocation);
	        vh::BufDestroyBuffer(GetDevice(), GetVmaAllocator(), geometry().m_vertexBuffer, geometry().m_vertexBufferAllocation);
		}

		for( auto ubo : m_registry.template GetView<vh::UniformBuffers&>() ) {
			vh::BufDestroyBuffer2(GetDevice(), GetVmaAllocator(), ubo);
		}

        vkDestroyCommandPool(GetDevice(), m_commandPool, nullptr);

        vkDestroyRenderPass(GetDevice(), m_renderPass, nullptr);

		vh::SynDestroyFences(GetDevice(), m_fences);

		vh::SynDestroySemaphores(GetDevice(), m_imageAvailableSemaphores, m_semaphores);

        vmaDestroyAllocator(GetVmaAllocator());

        vkDestroyDevice(GetDevice(), nullptr);

        vkDestroySurfaceKHR(GetInstance(), GetSurface(), nullptr);

		if (m_engine.GetDebug()) {
            vh::DevDestroyDebugUtilsMessengerEXT(GetInstance(), GetVulkanState()().m_debugMessenger, nullptr);
        }

        vkDestroyInstance(GetInstance(), nullptr);
		return false;
    }


	//-------------------------------------------------------------------------------------------------------


	bool RendererVulkan::OnTextureCreate( Message message ) {
		auto msg = message.template GetData<MsgTextureCreate>();
		auto handle = msg.m_handle;
		auto texture = m_registry.template Get<vh::Map&>(handle);
		auto pixels = texture().m_pixels;

		vh::ImgCreateTextureImage(GetPhysicalDevice(), GetDevice(), GetVulkanState()().m_vmaAllocator, GetGraphicsQueue(), m_commandPool, pixels, texture().m_width, texture().m_height, texture().m_size, texture);
		vh::ImgCreateTextureImageView(GetDevice(), texture);
		vh::ImgCreateTextureSampler(GetPhysicalDevice(), GetDevice(), texture);
		return false;
	}

	bool RendererVulkan::OnTextureDestroy( Message message ) {
		auto handle = message.template GetData<MsgTextureDestroy>().m_handle;
		auto texture = m_registry.template Get<vh::Map&>(handle);
		vkDestroySampler(GetDevice(), texture().m_mapSampler, nullptr);
		vkDestroyImageView(GetDevice(), texture().m_mapImageView, nullptr);
		vh::ImgDestroyImage(GetDevice(), GetVulkanState()().m_vmaAllocator, texture().m_mapImage, texture().m_mapImageAllocation);
		m_registry.Erase(handle);
		return false;
	}

	bool RendererVulkan::OnMeshCreate( Message message ) {
		auto handle = message.template GetData<MsgMeshCreate>().m_handle;
		auto mesh = m_registry.template Get<vh::Mesh&>(handle);
		vh::BufCreateVertexBuffer(GetPhysicalDevice(), GetDevice(), GetVulkanState()().m_vmaAllocator, GetGraphicsQueue(), m_commandPool, mesh);
		vh::BufCreateIndexBuffer( GetPhysicalDevice(), GetDevice(), GetVulkanState()().m_vmaAllocator, GetGraphicsQueue(), m_commandPool, mesh);
		return false;
	}

	bool RendererVulkan::OnMeshDestroy( Message message ) {
		auto handle = message.template GetData<MsgMeshDestroy>().m_handle;
		auto mesh = m_registry.template Get<vh::Mesh&>(handle);
		vh::BufDestroyBuffer(GetDevice(), GetVulkanState()().m_vmaAllocator, mesh().m_indexBuffer, mesh().m_indexBufferAllocation);
		vh::BufDestroyBuffer(GetDevice(), GetVulkanState()().m_vmaAllocator, mesh().m_vertexBuffer, mesh().m_vertexBufferAllocation);
		m_registry.Erase(handle);
		return false;
	}



};   // namespace vve