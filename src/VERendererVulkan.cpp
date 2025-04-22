#include "VHInclude2.h"
#include "VEInclude.h"


namespace vve {


	//-------------------------------------------------------------------------------------------------------
	// Vulkan Renderer

    RendererVulkan::RendererVulkan(std::string systemName, Engine& engine, std::string windowName ) 
        : Renderer(systemName, engine, windowName) {

        engine.RegisterCallbacks( { 
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
    }

    RendererVulkan::~RendererVulkan() {}

    bool RendererVulkan::OnExtensions(Message message) {
		auto msg = message.template GetData<MsgExtensions>();
		m_instanceExtensions.insert(m_instanceExtensions.end(), msg.m_instExt.begin(), msg.m_instExt.end());
		m_deviceExtensions.insert(m_deviceExtensions.end(), msg.m_devExt.begin(), msg.m_devExt.end());
		return false;
	}

    bool RendererVulkan::OnInit(Message message) {
		Renderer::OnInit(message);

		auto engineState = m_engine.GetState();

		if (engineState.debug) {
            m_instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

		volkInitialize();
		m_vkState().m_apiVersionInstance = engineState.apiVersion;
    	vvh::DevCreateInstance( {
			.m_validationLayers 	= m_validationLayers, 
			.m_instanceExtensions 	= m_instanceExtensions, 
			.m_name 				= engineState.name, 
			.m_apiVersion 			= engineState.apiVersion, 
			.m_debug 				= engineState.debug, 
			.m_instance 			= m_vkState().m_instance
		});
		
		volkLoadInstance(m_vkState().m_instance);

		if (engineState.debug) {
	        vvh::DevSetupDebugMessenger(m_vkState().m_instance, m_vkState().m_debugMessenger );
		}

		if (SDL_Vulkan_CreateSurface(m_windowSDLState().m_sdlWindow, m_vkState().m_instance, nullptr, &m_vkState().m_surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }

		m_vkState().m_apiVersionDevice = engineState.minimumVersion;
        
		vvh::DevPickPhysicalDevice( {
			.m_instance 		= m_vkState().m_instance, 
			.m_deviceExtensions = m_deviceExtensions, 
			.m_surface 			= m_vkState().m_surface, 
			.m_apiVersion 		= m_vkState().m_apiVersionDevice, 
			.m_physicalDevice 	= m_vkState().m_physicalDevice
		});
		
		
		uint32_t minor = std::min( VK_VERSION_MINOR(m_vkState().m_apiVersionDevice), VK_VERSION_MINOR(engineState.apiVersion) );
		minor = std::min( minor, VK_VERSION_MINOR(engineState.maximumVersion));
		if( minor < VK_VERSION_MINOR(engineState.minimumVersion) ) {
			std::cout << "No device found with Vulkan API version at least 1." << VK_VERSION_MINOR(engineState.minimumVersion) << "!\n";
			exit(1);
		}
		engineState.apiVersion = VK_MAKE_VERSION( VK_VERSION_MAJOR(engineState.apiVersion), minor, 0);
		vkGetPhysicalDeviceProperties(m_vkState().m_physicalDevice, &m_vkState().m_physicalDeviceProperties);
		vkGetPhysicalDeviceFeatures(m_vkState().m_physicalDevice, &m_vkState().m_physicalDeviceFeatures);

		vvh::DevCreateLogicalDevice( {
			.m_surface 			= m_vkState().m_surface, 
			.m_physicalDevice 	= m_vkState().m_physicalDevice, 
			.m_validationLayers = m_validationLayers, 
			.m_deviceExtensions = m_deviceExtensions, 
			.m_debug 			= engineState.debug, 
			.m_queueFamilies 	= m_vkState().m_queueFamilies, 
			.m_device 			= m_vkState().m_device, 
			.m_graphicsQueue 	= m_vkState().m_graphicsQueue, 
			.m_presentQueue 	= m_vkState().m_presentQueue
		});
        
		volkLoadDevice(m_vkState().m_device);
		
		vvh::DevInitVMA({
			.m_instance 		= m_vkState().m_instance, 
			.m_physicalDevice 	= m_vkState().m_physicalDevice, 
			.m_device 			= m_vkState().m_device, 
			.m_apiVersion 		= engineState.apiVersion, 
			.m_vmaAllocator 	= m_vkState().m_vmaAllocator
		});  

 		m_vkState().m_depthMapFormat = vvh::RenFindDepthFormat(m_vkState().m_physicalDevice);

       vvh::DevCreateSwapChain( {
			m_windowSDLState().m_sdlWindow, 
			m_vkState().m_surface, 
			m_vkState().m_physicalDevice, 
			m_vkState().m_device, 
			m_vkState().m_swapChain
		});
        
		vvh::DevCreateImageViews({m_vkState().m_device, m_vkState().m_swapChain});

		vvh::RenCreateRenderPass({
			.m_depthFormat 	= m_vkState().m_depthMapFormat, 
			.m_device 		= m_vkState().m_device, 
			.m_swapChain 	= m_vkState().m_swapChain, 
			.m_clear 		= m_clear,
			.m_renderPass 	= m_renderPass
		});

		vvh::RenCreateDescriptorSetLayout( { 
			.m_device = m_vkState().m_device, 
			.m_bindings = {}, 
			.m_descriptorSetLayout = m_descriptorSetLayoutPerFrame 
		} );
			
        vvh::RenCreateGraphicsPipeline( {
			.m_device 				= m_vkState().m_device, 
			.m_renderPass 			= m_renderPass, 
			.m_vertShaderPath 		= "shaders/Vulkan/vert.spv", 
			.m_fragShaderPath 		= "", 
			.m_bindingDescription 	= {}, 
			.m_attributeDescriptions = {},
			.m_descriptorSetLayouts = { m_descriptorSetLayoutPerFrame }, 
			.m_specializationConstants = {}, 
			.m_pushConstantRanges 	= {}, 
			.m_blendAttachments 	= {}, 
			.m_graphicsPipeline 	= m_graphicsPipeline
		});

		vvh::ComCreateCommandPool({
			.m_surface 			= m_vkState().m_surface, 
			.m_physicalDevice 	= m_vkState().m_physicalDevice, 
			.m_device 			= m_vkState().m_device,
			.m_commandPool 		= m_vkState().m_commandPool
		});

        vvh::ComCreateCommandPool({
			.m_surface 			= m_vkState().m_surface, 
			.m_physicalDevice 	= m_vkState().m_physicalDevice, 
			.m_device 			= m_vkState().m_device, 
			.m_commandPool 		= m_commandPool
		});
        
		vvh::RenCreateDepthResources({
			.m_physicalDevice 	= m_vkState().m_physicalDevice,
			.m_device 			= m_vkState().m_device, 
			.m_vmaAllocator 	= m_vkState().m_vmaAllocator, 
			.m_swapChain 		= m_vkState().m_swapChain, 
			.m_depthImage 		= m_vkState().m_depthImage
		});

		vvh::ImgTransitionImageLayout({
			.m_device 			= m_vkState().m_device, 
			.m_graphicsQueue 	= m_vkState().m_graphicsQueue, 
			.m_commandPool 		= m_commandPool,
			.m_image 			= m_vkState().m_depthImage.m_depthImage, 
			.m_format 			= m_vkState().m_depthMapFormat, 
			.m_aspect			= VK_IMAGE_ASPECT_DEPTH_BIT, 
			.m_mipLevels 		= 1, 
			.m_layers 			= 1, 
			.m_oldLayout 		= VK_IMAGE_LAYOUT_UNDEFINED, 
			.m_newLayout 		= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		});
        
		for( auto image : m_vkState().m_swapChain.m_swapChainImages ) {
			vvh::ImgTransitionImageLayout2({
				.m_device 			= m_vkState().m_device, 
				.m_graphicsQueue 	= m_vkState().m_graphicsQueue, 
				.m_commandPool 		= m_commandPool,
				.m_image 			= image, 
				.m_format 			= m_vkState().m_swapChain.m_swapChainImageFormat, 
				.m_oldLayout 		= VK_IMAGE_LAYOUT_UNDEFINED, 
				.m_newLayout 		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			});
		}

		vvh::RenCreateFramebuffers({
			.m_device 		= m_vkState().m_device, 
			.m_depthImage 	= m_vkState().m_depthImage, 
			.m_renderPass 	= m_renderPass,
			.m_swapChain 	= m_vkState().m_swapChain
		});

        vvh::ComCreateCommandBuffers({
			.m_device 			= m_vkState().m_device, 
			.m_commandPool 		= m_commandPool, 
			.m_commandBuffers 	= m_commandBuffers
		});
        vvh::RenCreateDescriptorPool({
			.m_device 			= m_vkState().m_device, 
			.m_sizes 			= 1000, 
			.m_descriptorPool 	= m_descriptorPool
		});
        vvh::SynCreateSemaphores({
			.m_device 					= m_vkState().m_device, 
			.m_imageAvailableSemaphores = m_imageAvailableSemaphores, 
			.m_renderFinishedSemaphores = m_renderFinishedSemaphores, 
			.m_size 					= 3, 
			.m_intermediateSemaphores 	= m_intermediateSemaphores
		});

		vvh::SynCreateFences({
			.m_device 	= m_vkState().m_device, 
			.m_size 	= MAX_FRAMES_IN_FLIGHT, 
			.m_fences 	= m_fences
		});
		return false;
    }

    bool RendererVulkan::OnPrepareNextFrame(Message message) {

        m_vkState().m_currentFrame = (m_vkState().m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
		m_vkState().m_commandBuffersSubmit.clear();

		vkWaitForFences(m_vkState().m_device, 1, &m_fences[m_vkState().m_currentFrame], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(
							m_vkState().m_device, 
							m_vkState().m_swapChain.m_swapChain, 
							UINT64_MAX,
            				m_imageAvailableSemaphores[m_vkState().m_currentFrame], 
							VK_NULL_HANDLE, 
							&m_vkState().m_imageIndex);

		vvh::ImgTransitionImageLayout2({
			.m_device 			= m_vkState().m_device, 
			.m_graphicsQueue 	= m_vkState().m_graphicsQueue, 
			.m_commandPool 		= m_commandPool, 
			.m_image 			= m_vkState().m_swapChain.m_swapChainImages[m_vkState().m_imageIndex], 
			.m_format 			= m_vkState().m_swapChain.m_swapChainImageFormat, 
			.m_oldLayout 		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 
			.m_newLayout 		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		});

        if (result == VK_ERROR_OUT_OF_DATE_KHR ) {
            vvh::DevRecreateSwapChain( {
				.m_window 			= m_windowSDLState().m_sdlWindow, 
				.m_surface 			= m_vkState().m_surface, 
				.m_physicalDevice 	= m_vkState().m_physicalDevice, 
				.m_device 			= m_vkState().m_device, 
				.m_vmaAllocator 	= m_vkState().m_vmaAllocator, 
				.m_swapChain 		= m_vkState().m_swapChain, 
				.m_depthImage 		= m_vkState().m_depthImage, 
				.m_renderPass 		= m_renderPass
			});

			for( auto image : m_vkState().m_swapChain.m_swapChainImages ) {
				vvh::ImgTransitionImageLayout2({
					.m_device 			= m_vkState().m_device, 
					.m_graphicsQueue 	= m_vkState().m_graphicsQueue, 
					.m_commandPool 		= m_commandPool,
					.m_image 			= image, 
					.m_format 			= m_vkState().m_swapChain.m_swapChainImageFormat, 
					.m_oldLayout 		= VK_IMAGE_LAYOUT_UNDEFINED, 
					.m_newLayout 		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
				});
			}
	
			m_engine.SendMsg( MsgWindowSize{} );
        } else assert (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
		return false;
    }

    bool RendererVulkan::OnRecordNextFrame(Message message) {

        vkResetCommandBuffer(m_commandBuffers[m_vkState().m_currentFrame],  0);

		vvh::ComBeginCommandBuffer({.m_commandBuffer = m_commandBuffers[m_vkState().m_currentFrame]});


		vvh::ComBeginRenderPass({
			.m_commandBuffer 	= m_commandBuffers[m_vkState().m_currentFrame], 
			.m_imageIndex 		= m_vkState().m_imageIndex, 
			.m_swapChain 		= m_vkState().m_swapChain, 
			.m_renderPass 		= m_renderPass, 
			.m_clear 			= m_clear, 
			.m_clearColor 		= m_windowState().m_clearColor, 
			.m_currentFrame 	= m_vkState().m_currentFrame
		});

		vvh::ComEndRenderPass( {.m_commandBuffer = m_commandBuffers[m_vkState().m_currentFrame]} );
		vvh::ComEndCommandBuffer({.m_commandBuffer = m_commandBuffers[m_vkState().m_currentFrame]});

		SubmitCommandBuffer(m_commandBuffers[m_vkState().m_currentFrame]);
		return false;
	}

    bool RendererVulkan::OnRenderNextFrame(Message message) {
        	
		vvh::ComSubmitCommandBuffers({
			m_vkState().m_device, 
			m_vkState().m_graphicsQueue, 
			m_vkState().m_commandBuffersSubmit, 
			m_imageAvailableSemaphores, 
			m_renderFinishedSemaphores, 
			m_intermediateSemaphores, 
			m_fences, 
			m_vkState().m_currentFrame
		});

		vvh::ImgTransitionImageLayout2({
			.m_device 			= m_vkState().m_device, 
			.m_graphicsQueue 	= m_vkState().m_graphicsQueue, 
			.m_commandPool 		= m_commandPool, 
			.m_image 			= m_vkState().m_swapChain.m_swapChainImages[m_vkState().m_imageIndex], 
			.m_format 			= m_vkState().m_swapChain.m_swapChainImageFormat, 
			.m_oldLayout 		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
			.m_newLayout 		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		});

		VkResult result = vvh::ComPresentImage({
			.m_presentQueue 	= m_vkState().m_presentQueue, 
			.m_swapChain 		= m_vkState().m_swapChain, 
			.m_imageIndex 		= m_vkState().m_imageIndex, 
			.m_signalSemaphore 	= m_renderFinishedSemaphores[m_vkState().m_currentFrame]
		});

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_vkState().m_framebufferResized) {
            m_vkState().m_framebufferResized = false;
            vvh::DevRecreateSwapChain({
				.m_window 			= m_windowSDLState().m_sdlWindow, 
				.m_surface 			= m_vkState().m_surface, 
				.m_physicalDevice 	= m_vkState().m_physicalDevice, 
				.m_device 			= m_vkState().m_device, 
				.m_vmaAllocator 	= m_vkState().m_vmaAllocator, 
				.m_swapChain 		= m_vkState().m_swapChain, 
				.m_depthImage 		= m_vkState().m_depthImage, 
				.m_renderPass 		= m_renderPass
			});

			for( auto image : m_vkState().m_swapChain.m_swapChainImages ) {
				vvh::ImgTransitionImageLayout2({
					.m_device 			= m_vkState().m_device, 
					.m_graphicsQueue 	= m_vkState().m_graphicsQueue, 
					.m_commandPool 		= m_commandPool,
					.m_image 			= image, 
					.m_format 			= m_vkState().m_swapChain.m_swapChainImageFormat, 
					.m_oldLayout 		= VK_IMAGE_LAYOUT_UNDEFINED, 
					.m_newLayout 		= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
				});
			}
	
			m_engine.SendMsg( MsgWindowSize{} );
        } else assert(result == VK_SUCCESS);
		return false;
    }
    
    bool RendererVulkan::OnQuit(Message message) {
        vkDeviceWaitIdle(m_vkState().m_device);

        vvh::DevCleanupSwapChain({
			.m_device 		= m_vkState().m_device, 
			.m_vmaAllocator = m_vkState().m_vmaAllocator, 
			.m_swapChain 	= m_vkState().m_swapChain, 
			.m_depthImage 	= m_vkState().m_depthImage
		});

        vkDestroyPipeline(m_vkState().m_device, m_graphicsPipeline.m_pipeline, nullptr);
        vkDestroyPipelineLayout(m_vkState().m_device, m_graphicsPipeline.m_pipelineLayout, nullptr);

        vkDestroyDescriptorPool(m_vkState().m_device, m_descriptorPool, nullptr);

		for( decltype(auto) texture : m_registry.template GetView<vvh::Image&>() ) {
			vkDestroySampler(m_vkState().m_device, texture().m_mapSampler, nullptr);
        	vkDestroyImageView(m_vkState().m_device, texture().m_mapImageView, nullptr);
	        vvh::ImgDestroyImage({
				.m_device 			= m_vkState().m_device, 
				.m_vmaAllocator 	= m_vkState().m_vmaAllocator, 
				.m_image 			= texture().m_mapImage,
				.m_imageAllocation 	= texture().m_mapImageAllocation
			});
		}

		vkDestroyDescriptorSetLayout(m_vkState().m_device, m_descriptorSetLayoutPerFrame, nullptr);

		for( auto geometry : m_registry.template GetView<vvh::Mesh&>() ) {
	        vvh::BufDestroyBuffer({
				.m_device 		= m_vkState().m_device, 
				.m_vmaAllocator = m_vkState().m_vmaAllocator, 
				.m_buffer 		= geometry().m_indexBuffer, 
				.m_allocation 	= geometry().m_indexBufferAllocation
			});
	        vvh::BufDestroyBuffer({
				.m_device 		= m_vkState().m_device, 
				.m_vmaAllocator = m_vkState().m_vmaAllocator, 
				.m_buffer 		= geometry().m_vertexBuffer, 
				.m_allocation 	= geometry().m_vertexBufferAllocation
			});
		}

		for( auto ubo : m_registry.template GetView<vvh::Buffer&>() ) {
			vvh::BufDestroyBuffer2({
				.m_device 			= m_vkState().m_device, 
				.m_vmaAllocator 	= m_vkState().m_vmaAllocator, 
				.m_buffers 			= ubo
			});
		}

        vkDestroyCommandPool(m_vkState().m_device, m_commandPool, nullptr);
        vkDestroyCommandPool(m_vkState().m_device, m_vkState().m_commandPool, nullptr);

        vkDestroyRenderPass(m_vkState().m_device, m_renderPass, nullptr);

		vvh::SynDestroyFences({m_vkState().m_device, m_fences});

		vvh::SynDestroySemaphores({
			m_vkState().m_device, 
			m_imageAvailableSemaphores, 
			m_renderFinishedSemaphores, 
			m_intermediateSemaphores
		});

        vmaDestroyAllocator(m_vkState().m_vmaAllocator);

        vkDestroyDevice(m_vkState().m_device, nullptr);

        vkDestroySurfaceKHR(m_vkState().m_instance, m_vkState().m_surface, nullptr);

		if (m_engine.GetState().debug) {
            vvh::DevDestroyDebugUtilsMessengerEXT({
				.m_instance 		= m_vkState().m_instance, 
				.m_debugMessenger 	= m_vkState().m_debugMessenger, 
				.m_pAllocator 		= nullptr
			});
        }

        vkDestroyInstance(m_vkState().m_instance, nullptr);
		return false;
    }

	//-------------------------------------------------------------------------------------------------------

	bool RendererVulkan::OnTextureCreate( Message message ) {
		auto msg = message.template GetData<MsgTextureCreate>();
		auto handle = msg.m_handle;
		auto texture = m_registry.template Get<vvh::Image&>(handle);
		auto pixels = texture().m_pixels;

		vvh::ImgCreateTextureImage({
			m_vkState().m_physicalDevice, 
			m_vkState().m_device, 
			m_vkState().m_vmaAllocator, 
			m_vkState().m_graphicsQueue, 
			m_commandPool, 
			pixels, 
			texture().m_width, 
			texture().m_height, 
			texture().m_size, 
			texture
		});
		vvh::ImgCreateTextureImageView({m_vkState().m_device, texture});
		vvh::ImgCreateTextureSampler({m_vkState().m_physicalDevice, m_vkState().m_device, texture});
		return false;
	}

	bool RendererVulkan::OnTextureDestroy( Message message ) {
		auto handle = message.template GetData<MsgTextureDestroy>().m_handle;
		auto texture = m_registry.template Get<vvh::Image&>(handle);
		vkDestroySampler(m_vkState().m_device, texture().m_mapSampler, nullptr);
		vkDestroyImageView(m_vkState().m_device, texture().m_mapImageView, nullptr);
		vvh::ImgDestroyImage({
			.m_device			= m_vkState().m_device, 
			.m_vmaAllocator 	= m_vkState().m_vmaAllocator, 
			.m_image 			= texture().m_mapImage, 
			.m_imageAllocation 	= texture().m_mapImageAllocation
		});
		m_registry.Erase(handle);
		return false;
	}

	bool RendererVulkan::OnMeshCreate( Message message ) {
		auto handle = message.template GetData<MsgMeshCreate>().m_handle;
		auto mesh = m_registry.template Get<vvh::Mesh&>(handle);
		vvh::BufCreateVertexBuffer({
			m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, m_vkState().m_graphicsQueue, m_commandPool, mesh
		});
		vvh::BufCreateIndexBuffer({
			 m_vkState().m_physicalDevice, m_vkState().m_device, m_vkState().m_vmaAllocator, m_vkState().m_graphicsQueue, m_commandPool, mesh
		});
		return false;
	}

	bool RendererVulkan::OnMeshDestroy( Message message ) {
		auto handle = message.template GetData<MsgMeshDestroy>().m_handle;
		auto mesh = m_registry.template Get<vvh::Mesh&>(handle);
		vvh::BufDestroyBuffer({
			.m_device 		= m_vkState().m_device, 
			.m_vmaAllocator = m_vkState().m_vmaAllocator, 
			.m_buffer 		= mesh().m_indexBuffer, 
			.m_allocation 	= mesh().m_indexBufferAllocation
		});
		vvh::BufDestroyBuffer({
			.m_device 		= m_vkState().m_device, 
			.m_vmaAllocator = m_vkState().m_vmaAllocator, 
			.m_buffer 		= mesh().m_vertexBuffer, 
			.m_allocation 	= mesh().m_vertexBufferAllocation
		});
		m_registry.Erase(handle);
		return false;
	}



};   // namespace vve