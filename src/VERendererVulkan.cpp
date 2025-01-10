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
    	m_windowSDL = (WindowSDL*)m_window;
		if (m_engine.GetDebug()) {
            m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

    	vh::createInstance( m_validationLayers, m_instanceExtensions, m_instance);
		if (m_engine.GetDebug()) {
	        vh::setupDebugMessenger(GetInstance(), m_debugMessenger);
		}

		if (SDL_Vulkan_CreateSurface(m_windowSDL->GetSDLWindow(), GetInstance(), &m_surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }

        vh::pickPhysicalDevice(GetInstance(), m_deviceExtensions, GetSurface(), m_physicalDevice);
        vh::createLogicalDevice(GetSurface(), GetPhysicalDevice(), m_queueFamilies, m_validationLayers, 
			m_deviceExtensions, m_device, m_graphicsQueue, m_presentQueue);
        vh::initVMA(GetInstance(), GetPhysicalDevice(), GetDevice(), m_vmaAllocator);  
        vh::createSwapChain(m_windowSDL->GetSDLWindow(), GetSurface(), GetPhysicalDevice(), GetDevice(), m_swapChain);
        vh::createImageViews(GetDevice(), GetSwapChain());
        vh::createRenderPassClear(GetPhysicalDevice(), GetDevice(), GetSwapChain(), true, m_renderPass);
        
		vh::createDescriptorSetLayout(GetDevice(), //Per object
			{{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
			{ .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT} },
			m_descriptorSetLayoutPerObject
		);

		vh::createDescriptorSetLayout( GetDevice(), //Per frame
			{{ .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT } },
			m_descriptorSetLayoutPerFrame );
			
        vh::createGraphicsPipeline(GetDevice(), GetRenderPass(), "shaders\\Vulkan\\vert.spv", "shaders\\Vulkan\\frag.spv",
			 { m_descriptorSetLayoutPerFrame, m_descriptorSetLayoutPerObject }, m_graphicsPipeline);

        vh::createCommandPool(GetSurface(), GetPhysicalDevice(), GetDevice(), m_commandPool);
        vh::createDepthResources(GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetSwapChain(), m_depthImage);
        vh::createFramebuffers(GetDevice(), GetSwapChain(), GetDepthImage(), GetRenderPass());

        vh::createCommandBuffers(GetDevice(), GetCommandPool(), m_commandBuffers);
        vh::createDescriptorPool(GetDevice(), 1000, m_descriptorPool);
        vh::createSemaphores(GetDevice(), 3, m_imageAvailableSemaphores, m_semaphores);
		vh::createFences(GetDevice(), MAX_FRAMES_IN_FLIGHT, m_fences);
		return false;
    }

    bool RendererVulkan::OnPrepareNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return false;

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		m_commandBuffersSubmit.clear();

		vkWaitForFences(GetDevice(), 1, &m_fences[GetCurrentFrame()], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(GetDevice(), GetSwapChain().m_swapChain, UINT64_MAX,
                            m_imageAvailableSemaphores[GetCurrentFrame()], VK_NULL_HANDLE, &m_imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
            recreateSwapChain(m_windowSDL->GetSDLWindow(), GetSurface(), GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage(), GetRenderPass());
        } else assert (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
		return false;
    }

    bool RendererVulkan::OnRecordNextFrame(Message message) {
		if(m_windowSDL->GetIsMinimized()) return false;

        vkResetCommandBuffer(m_commandBuffers[GetCurrentFrame()],  0);

		vh::startRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()], GetImageIndex(), 
			GetSwapChain(), GetRenderPass(), m_graphicsPipeline, 
			true, ((WindowSDL*)m_window)->GetClearColor(), GetCurrentFrame());

		vh::endRecordCommandBuffer(m_commandBuffers[GetCurrentFrame()]);

		SubmitCommandBuffer(m_commandBuffers[GetCurrentFrame()]);
		return false;
	}

    bool RendererVulkan::OnRenderNextFrame(Message message) {
        if(m_window->GetIsMinimized()) return false;
        
		VkSemaphore signalSemaphore;
		vh::submitCommandBuffers(GetDevice(), GetGraphicsQueue(), m_commandBuffersSubmit, 
			m_imageAvailableSemaphores, m_semaphores, signalSemaphore, m_fences, GetCurrentFrame());
				
   		vh::transitionImageLayout(GetDevice(), GetGraphicsQueue(), GetCommandPool(), 
			GetSwapChain().m_swapChainImages[GetImageIndex()], GetSwapChain().m_swapChainImageFormat, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		VkResult result = vh::presentImage(GetPresentQueue(), GetSwapChain(), GetImageIndex(), signalSemaphore);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
            m_framebufferResized = false;
            vh::recreateSwapChain(m_windowSDL->GetSDLWindow(), GetSurface(), GetPhysicalDevice(), GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage(), GetRenderPass());
        } else assert(result == VK_SUCCESS);
		return false;
    }
    
    bool RendererVulkan::OnQuit(Message message) {

        vkDeviceWaitIdle(GetDevice());

        vh::cleanupSwapChain(GetDevice(), GetVmaAllocator(), GetSwapChain(), GetDepthImage());

        vkDestroyPipeline(GetDevice(), m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(GetDevice(), m_graphicsPipeline.m_pipelineLayout, nullptr);

        vkDestroyDescriptorPool(GetDevice(), GetDescriptorPool(), nullptr);

		for( decltype(auto) texture : m_registry.template GetView<vh::Texture&>() ) {
			vkDestroySampler(GetDevice(), texture.m_textureSampler, nullptr);
        	vkDestroyImageView(GetDevice(), texture.m_textureImageView, nullptr);
	        vh::destroyImage(GetDevice(), GetVmaAllocator(), texture.m_textureImage, texture.m_textureImageAllocation);
		}

		vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayoutPerObject, nullptr);
		vkDestroyDescriptorSetLayout(GetDevice(), m_descriptorSetLayoutPerFrame, nullptr);

		for( auto geometry : m_registry.template GetView<vh::Geometry&>() ) {
	        vh::destroyBuffer(GetDevice(), GetVmaAllocator(), geometry.m_indexBuffer, geometry.m_indexBufferAllocation);
	        vh::destroyBuffer(GetDevice(), GetVmaAllocator(), geometry.m_vertexBuffer, geometry.m_vertexBufferAllocation);
		}

		for( auto ubo : m_registry.template GetView<vh::UniformBuffers&>() ) {
			vh::destroyBuffer2(GetDevice(), GetVmaAllocator(), ubo);
		}

        vkDestroyCommandPool(GetDevice(), GetCommandPool(), nullptr);

        vkDestroyRenderPass(GetDevice(), GetRenderPass(), nullptr);

		vh::destroyFences(GetDevice(), m_fences);

		vh::destroySemaphores(GetDevice(), m_imageAvailableSemaphores, m_semaphores);

        vmaDestroyAllocator(GetVmaAllocator());

        vkDestroyDevice(GetDevice(), nullptr);

        vkDestroySurfaceKHR(GetInstance(), GetSurface(), nullptr);

		if (m_engine.GetDebug()) {
            vh::DestroyDebugUtilsMessengerEXT(GetInstance(), m_debugMessenger, nullptr);
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
		vh::createTextureImage(GetPhysicalDevice(), GetDevice(), m_vmaAllocator, GetGraphicsQueue(), GetCommandPool(), pixels, texture.m_width, texture.m_height, texture.m_size, texture);
		vh::createTextureImageView(GetDevice(), texture);
		vh::createTextureSampler(GetPhysicalDevice(), GetDevice(), texture);
		return false;
	}

	bool RendererVulkan::OnTextureDestroy( Message message ) {
		auto handle = message.template GetData<MsgTextureDestroy>().m_handle;
		auto& texture = m_registry.template Get<vh::Texture&>(handle);
		vkDestroySampler(GetDevice(), texture.m_textureSampler, nullptr);
		vkDestroyImageView(GetDevice(), texture.m_textureImageView, nullptr);
		vh::destroyImage(GetDevice(), m_vmaAllocator, texture.m_textureImage, texture.m_textureImageAllocation);
		m_registry.Erase(handle);
		return false;
	}

	bool RendererVulkan::OnGeometryCreate( Message message ) {
		auto handle = message.template GetData<MsgGeometryCreate>().m_handle;
		auto& geometry = m_registry.template Get<vh::Geometry&>(handle);
		vh::createVertexBuffer(GetPhysicalDevice(), GetDevice(), m_vmaAllocator, GetGraphicsQueue(), GetCommandPool(), geometry);
		vh::createIndexBuffer( GetPhysicalDevice(), GetDevice(), m_vmaAllocator, GetGraphicsQueue(), GetCommandPool(), geometry);
		return false;
	}

	bool RendererVulkan::OnGeometryDestroy( Message message ) {
		auto handle = message.template GetData<MsgGeometryDestroy>().m_handle;
		auto& geometry = m_registry.template Get<vh::Geometry&>(handle);
		vh::destroyBuffer(GetDevice(), m_vmaAllocator, geometry.m_indexBuffer, geometry.m_indexBufferAllocation);
		vh::destroyBuffer(GetDevice(), m_vmaAllocator, geometry.m_vertexBuffer, geometry.m_vertexBufferAllocation);
		m_registry.Erase(handle);
		return false;
	}



};   // namespace vve