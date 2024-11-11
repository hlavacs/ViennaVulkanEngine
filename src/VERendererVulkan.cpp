

#include "VERendererVulkan.h"
#include "VEEngine.h"
#include "VEWindowSDL.h"


namespace vve {

    template<ArchitectureType ATYPE>
    RendererVulkan<ATYPE>::RendererVulkan(Engine<ATYPE>* engine, Window<ATYPE>* window, std::string name ) 
        : Renderer<ATYPE>(engine, window, name) {

        engine->RegisterSystem( { 
			{this, -50000, MessageType::INIT, [this](Message message){this->OnInit(message);} }, 
			{this, -50000, MessageType::PREPARE_NEXT_FRAME, [this](Message message){this->OnPrepareNextFrame(message);} },
			{this, -50000, MessageType::RECORD_NEXT_FRAME, [this](Message message){this->OnRecordNextFrame(message);} },
			{this, -50000, MessageType::RENDER_NEXT_FRAME, [this](Message message){this->OnRenderNextFrame(message);} },
			{this,     50, MessageType::INIT, [this](Message message){this->OnInit2(message);} },
			{this,  50000, MessageType::QUIT, [this](Message message){this->OnQuit(message);} }
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
        vh::createRenderPass(m_physicalDevice, m_device, m_swapChain, m_renderPass);
        vh::createDescriptorSetLayout(m_device, m_descriptorSetLayout);
        vh::createGraphicsPipeline(m_device, m_renderPass, m_descriptorSetLayout, m_graphicsPipeline);
        vh::createCommandPool(m_window->GetSurface(), m_physicalDevice, m_device, m_commandPool);
        vh::createDepthResources(m_physicalDevice, m_device, m_vmaAllocator, m_swapChain, m_depthImage);
        vh::createFramebuffers(m_device, m_swapChain, m_depthImage, m_renderPass);
        vh::createTextureImage(m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, m_texture);
        vh::createTextureImageView(m_device, m_texture);
        vh::createTextureSampler(m_physicalDevice, m_device, m_texture);
		vh::loadModel(m_geometry);
        vh::createVertexBuffer(m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, m_geometry);
        vh::createIndexBuffer( m_physicalDevice, m_device, m_vmaAllocator, m_graphicsQueue, m_commandPool, m_geometry);
        vh::createUniformBuffers(m_physicalDevice, m_device, m_vmaAllocator, m_uniformBuffers);
        vh::createDescriptorPool(m_device, m_descriptorPool);
        vh::createDescriptorSets(m_device, m_texture, m_descriptorSetLayout, m_uniformBuffers, m_descriptorPool, m_descriptorSets);
        vh::createCommandBuffers(m_device, m_commandPool, m_commandBuffers);
        vh::createSyncObjects(m_device, m_syncObjects);

    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnPrepareNextFrame(Message message) {

    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnRecordNextFrame(Message message) {

    }

    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnRenderNextFrame(Message message) {

    }
    
    template<ArchitectureType ATYPE>
    void RendererVulkan<ATYPE>::OnQuit(Message message) {
		if (m_engine->GetDebug()) {
            vh::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }
    }

    template class RendererVulkan<ArchitectureType::SEQUENTIAL>;
    template class RendererVulkan<ArchitectureType::PARALLEL>;

};   // namespace vve