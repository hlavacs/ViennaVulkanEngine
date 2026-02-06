/**
 * @file vrt_renderer.cpp
 * @brief RendererRayTraced implementation.
 */

#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {
    RendererRayTraced::RendererRayTraced(std::string systemName, Engine& engine, std::string windowName)
        : System{ systemName, engine }, m_windowName(windowName), gen(std::random_device{}()), dist(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max()) {

        
        engine.RegisterCallbacks({
            {this,  3500, "INIT", [this](Message& message) { return OnInit(message); } },
            {this,  2000, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); } },
            {this,  2000, "RECORD_NEXT_FRAME", [this](Message& message) { return OnRecordNextFrame(message); } },
            {this,      0, "RENDER_NEXT_FRAME", [this](Message& message) { return OnRenderNextFrame(message); } },
            {this,     0, "QUIT", [this](Message& message) { return OnQuit(message); } },
            });
            
    };

    bool RendererRayTraced::OnQuit(Message message) {
        vkDeviceWaitIdle(device);
        //Free descriptor sets
        vkFreeDescriptorSets(device, descriptorPool, descriptorSets.size(), descriptorSets.data());
        vkFreeDescriptorSets(device, descriptorPoolRT, descriptorSetsRT.size(), descriptorSetsRT.data());
        vkFreeDescriptorSets(device, descriptorPoolTargets, descriptorSetsTargets.size(), descriptorSetsTargets.data());

        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayoutRT, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayoutTargets, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorPool(device, descriptorPoolRT, nullptr);
        vkDestroyDescriptorPool(device, descriptorPoolTargets, nullptr);

        //piplines
        rasterizer->freeResources();
        raytracer->freeResources();

        //buffers
        lightManager->freeResources();
        textureManager->freeResources();
        materialManager->freeResources();
        objectManager->freeResources();
        commandManager->freeResources();

        for (HostBuffer<UniformBufferObject>* buffer : uniformBuffer_c) {
            delete buffer;
        }

        //render targets
        for (RenderTarget* target : allTargets) {
            delete target;
        }

        delete swapchain;
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyDebugUtilsMessengerEXT(instance, vkbInstance.debug_messenger, nullptr);
        vkDestroyInstance(instance, nullptr);

        return false;
    }

    RendererRayTraced::~RendererRayTraced() { 
        std::cout << "main destructor called \n";      
    }




    void RendererRayTraced::destroyGeneralDescriptors() {
        vkDeviceWaitIdle(device);
        if (generalDiscriptorsCreated) {
            vkFreeDescriptorSets(
                device,
                descriptorPool,
                descriptorSets.size(),
                descriptorSets.data()
            );
        }
    }
    void RendererRayTraced::destroyRayTracingDescriptors() {
        vkDeviceWaitIdle(device);
        if (raytracingDiscriptorsCreated) {
            vkFreeDescriptorSets(
                device,
                descriptorPoolRT,
                descriptorSetsRT.size(),
                descriptorSetsRT.data()
            );
        }
    }
    void RendererRayTraced::recreateGeneralDescriptors() {    
        createDescriptorSets(descriptorSets, descriptorPool, descriptorSetLayout, uniformBuffer_c, materialManager->getMaterialBuffer(), textureManager->getTextures(), textureManager->getSampler(), device);
        rasterizer->setDescriptorSets(descriptorSets);
        raytracer->setDescriptorSets(descriptorSets, descriptorSetsRT);  
        generalDiscriptorsCreated = true;
    }
    void RendererRayTraced::recreateRayTracingDescriptors() {
        createDescriptorSetsRT(descriptorSetsRT, descriptorPoolRT, descriptorSetLayoutRT, objectManager->getTlas().accel, objectManager->getVertexBuffer(), objectManager->getIndexBuffer(), objectManager->getInstanceBuffers(), lightManager->getLightBuffer(), device);
        raytracer->setDescriptorSets(descriptorSets, descriptorSetsRT);
        raytracingDiscriptorsCreated = true;
    }

    bool RendererRayTraced::OnInit(Message message) {
        auto [handle, stateW, stateSDL] = WindowSDL::GetState(m_registry);
        m_windowState = stateW;
        m_windowSDLState = stateSDL;

        createInstance();

        if (SDL_Vulkan_CreateSurface(m_windowSDLState().m_sdlWindow, instance, nullptr, &surface) == 0) {
            printf("Failed to create Vulkan surface.\n");
        }

        pickPhysicalDevice();
        createLogicalDevice();

        commandManager = new CommandManager(device, physicalDevice, surface, graphicsQueue);
        swapchain = new SwapChain(physicalDevice, device, surface, presentQueue, commandManager, m_windowSDLState().m_sdlWindow);

        //textureManager = new TextureManager(device, physicalDevice, commandManager);

        auto textureManagerUnique = std::make_unique<TextureManager>("Texture Manager", m_engine, device, physicalDevice, commandManager);
        textureManager = textureManagerUnique.get();
        m_engine.RegisterSystem(std::move(textureManagerUnique));

        auto materialManagerUnique = std::make_unique<MaterialManager>("Material Manager", m_engine, device, physicalDevice, commandManager);
        materialManager = materialManagerUnique.get();
        m_engine.RegisterSystem(std::move(materialManagerUnique));

        //materialManager->createMaterialBuffer();
        //objectManager = new ObjectManager("Object Manager",m_engine, device, physicalDevice, commandManager, m_asProperties);

        auto objectManagerUnique = std::make_unique<ObjectManager>("Object Manager", m_engine, device, physicalDevice, commandManager, m_asProperties);
        objectManager = objectManagerUnique.get();  
        m_engine.RegisterSystem(std::move(objectManagerUnique));


        auto lightManagerUnique = std::make_unique<LightManager>("Light Manager", m_engine, commandManager, device, physicalDevice);
        lightManager = lightManagerUnique.get();
        m_engine.RegisterSystem(std::move(lightManagerUnique));

        uniformBuffer_c.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            uniformBuffer_c[i] = new HostBuffer<UniformBufferObject>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0, device, physicalDevice);
        }

        createDescriptorSetLayout(descriptorSetLayout, device);
        createDescriptorPool(descriptorPool, device);
       
        auto piplineRasterizedUnique = std::make_unique<PiplineRasterized>("Pipline Rasterized", m_engine, device, swapchain->getExtent(), commandManager, objectManager->getVertexBuffer(), objectManager->getIndexBuffer(), objectManager->getInstanceBuffers(), descriptorSetLayout);
        rasterizer = piplineRasterizedUnique.get();
        m_engine.RegisterSystem(std::move(piplineRasterizedUnique));


        //rasterizer = new PiplineRasterized(device, swapchain->getExtent(), commandManager, objectManager->getVertexBuffer(), objectManager->getIndexBuffer(), objectManager->getInstanceBuffers(), descriptorSetLayout, descriptorSets);

        //mainTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, swapchain->getFormat(), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        depthTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, commandManager, device, physicalDevice);


        albedoTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        normalTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        specTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        positionTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice, VkClearColorValue{{-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),1.0}});
        shadingNormalTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);

        //rasterizer->bindRenderTarget(mainTarget);
        rasterizer->bindRenderTarget(albedoTarget);
        rasterizer->bindRenderTarget(normalTarget);
        rasterizer->bindRenderTarget(specTarget);
        rasterizer->bindRenderTarget(positionTarget);
        rasterizer->bindRenderTarget(shadingNormalTarget);

        rasterizer->bindDepthRenderTarget(depthTarget);
        rasterizer->initGraphicsPipeline();

        RtTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);

        allTargets.push_back(albedoTarget);
        allTargets.push_back(normalTarget);
        allTargets.push_back(specTarget);
        allTargets.push_back(positionTarget);
        allTargets.push_back(RtTarget);
        allTargets.push_back(depthTarget);
        allTargets.push_back(shadingNormalTarget);


        rayTracingTargets.push_back(albedoTarget);
        rayTracingTargets.push_back(normalTarget);
        rayTracingTargets.push_back(specTarget);
        rayTracingTargets.push_back(positionTarget);
        rayTracingTargets.push_back(shadingNormalTarget);

        rayTracingTargets.push_back(RtTarget);

        createDescriptorSetLayoutRT(descriptorSetLayoutRT, device);
        createDescriptorPoolRT(descriptorPoolRT, device);

        createDescriptorSetLayoutTargets(descriptorSetLayoutTargets, rayTracingTargets.size(), device);
        createDescriptorPoolTargets(descriptorPoolTargets, rayTracingTargets.size(), device);
        createDescriptorSetsTargets(descriptorSetsTargets, descriptorPoolTargets, descriptorSetLayoutTargets, rayTracingTargets, device);

        raytracer = new PiplineRaytraced(device, physicalDevice, commandManager, m_rtProperties, descriptorSetLayout, descriptorSetLayoutRT, descriptorSetLayoutTargets, descriptorSetsTargets, swapchain->getExtent());

        raytracer->bindRenderTarget(albedoTarget);
        raytracer->bindRenderTarget(normalTarget);
        raytracer->bindRenderTarget(specTarget);
        raytracer->bindRenderTarget(positionTarget);
        raytracer->bindRenderTarget(shadingNormalTarget);

        raytracer->bindRenderTarget(RtTarget);

        raytracer->initRayTracingPipeline();

        //upload data to VkState

		auto view = m_registry.GetView<vecs::Handle, VulkanState&>();
		auto iterBegin = view.begin();
		auto iterEnd = view.end();
		if( !(iterBegin != iterEnd)) {
			m_vulkanStateHandle = m_registry.Insert(VulkanState{});
			m_vkState = m_registry.Get<VulkanState&>(m_vulkanStateHandle);
			return false;
		}
		auto [handleV, stateV] = *iterBegin;
		m_vulkanStateHandle = handleV;
		m_vkState = stateV;


        m_vkState().m_instance = instance;
        m_vkState().m_device = device;

        vvh::SwapChain engineSwapchain;
        engineSwapchain.m_swapChain = swapchain->getSwapchain();
        engineSwapchain.m_swapChainExtent = swapchain->getExtent();
        engineSwapchain.m_swapChainImageFormat = swapchain->getFormat();
        m_vkState().m_swapChain = engineSwapchain;
        m_vkState().m_depthMapFormat = VK_FORMAT_D32_SFLOAT;

        m_vkState().m_physicalDevice = physicalDevice;
        m_vkState().m_graphicsQueue = graphicsQueue;

        vvh::QueueFamilyIndices indices;
        indices.graphicsFamily = graphicsQueueIndex;
        indices.presentFamily = presentQueueIndex;
        m_vkState().m_queueFamilies = indices;

        m_vkState().m_surface = surface;

       

        return false;
    }

    void RendererRayTraced::resizeWindow() {
        swapchain->recreateSwapChain();

        for (RenderTarget* target : allTargets) {
            target->recreateRenderTarget(swapchain->getExtent().width, swapchain->getExtent().height);
        }
        rasterizer->recreateFrameBuffers(swapchain->getExtent());

        vkFreeDescriptorSets(
            device,
            descriptorPoolTargets,
            descriptorSetsTargets.size(),
            descriptorSetsTargets.data()
        );
        createDescriptorSetsTargets(descriptorSetsTargets, descriptorPoolTargets, descriptorSetLayoutTargets, rayTracingTargets, device);
        raytracer->setRenderTargetsDescriptorSets(descriptorSetsTargets);
        raytracer->setExtent(swapchain->getExtent());
        m_engine.SendMsg(MsgWindowSize{});
        //send Message window resized maybe (aspect ratio incorrect)
    }

    bool RendererRayTraced::OnRecordNextFrame(Message message) {
        VkResult result = swapchain->acquireNextImage(currentFrame);
        m_vkState().m_imageIndex = swapchain->getImageIndex(currentFrame);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            resizeWindow();
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        commandManager->beginCommand(currentFrame);
        rasterizer->recordCommandBuffer(currentFrame);
        //the raytracer switches the sampler of the textures to linear causing the image to darken. In generell later on everything in the renderer should be switched to linear and only be converted to srgb before presentation
        raytracer->recordCommandBuffer(currentFrame);
        swapchain->recordImageTransfer(currentFrame, RtTarget);
        //swapchain->recordImageTransfer(currentFrame, albedoTarget);

        return false;
    }

    void RendererRayTraced::updateUniformBuffer(uint32_t currentImage) {

        auto [lToW, view, proj] = *m_registry.GetView<LocalToWorldMatrix&, ViewMatrix&, ProjectionMatrix&>().begin();

        UniformBufferObject ubo{};

        ubo.view = view();
        ubo.proj = proj();

        ubo.viewInv = glm::transpose(glm::inverse(ubo.view)); // Transpose inverse view matrix
        ubo.projInv = glm::transpose(glm::inverse(ubo.proj));

        ubo.seed = dist(gen);

        uniformBuffer_c[currentImage]->updateBuffer(&ubo, 1);
    }

    bool RendererRayTraced::OnPrepareNextFrame(Message message) {
        commandManager->waitForFence(currentFrame);
        updateUniformBuffer(currentFrame);

        if (materialManager->materialChanged() || textureManager->texturesChanged()) {
            destroyGeneralDescriptors();
        }
        if (objectManager->meshesChanged() || objectManager->instancesChanged() || lightManager->lightsChanged()) {      
            destroyRayTracingDescriptors();
        }

        lightManager->prepareNextFrame();
        materialManager->prepareNextFrame();
        objectManager->prepareNextFrame();

        if (materialManager->materialChanged() || textureManager->texturesChanged()) {
            recreateGeneralDescriptors();
        }
        if (objectManager->meshesChanged() || objectManager->instancesChanged() || lightManager->lightsChanged()) {
            recreateRayTracingDescriptors();
        }

        return false;
    }
    

    bool RendererRayTraced::OnRenderNextFrame(Message message) {

        commandManager->executeCommand(currentFrame, swapchain->getImageAvailableSemaphore(currentFrame));

        VkResult result = swapchain->presentImage(currentFrame, commandManager->getRenderFinishedSemaphores(currentFrame));

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            resizeWindow();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        objectManager->updateCurrentFrame(currentFrame);

        m_vkState().m_currentFrame = currentFrame;

        return false;
    }

    void RendererRayTraced::createInstance() {


        Uint32 count_instance_extensions;
        const char* const* instance_extensions = SDL_Vulkan_GetInstanceExtensions(&count_instance_extensions);

        if (instance_extensions == NULL) {
            std::cerr << "SDL_Vulkan_GetInstanceExtensions (count) failed: " << SDL_GetError() << std::endl;
        }

        std::vector<const char*> extensions(instance_extensions, instance_extensions + count_instance_extensions);

        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        vkb::InstanceBuilder instance_builder;

        auto system_info_ret = vkb::SystemInfo::get_system_info();
        if (!system_info_ret) { /* report error */ }
        auto system_info = system_info_ret.value();

        instance_builder
            .set_app_name("Vulkan Application")
            .set_engine_name("No Engine")
            .require_api_version(1, 2, 0);

        if (system_info.validation_layers_available) {
            instance_builder.enable_validation_layers().use_default_debug_messenger();
        }

        for (auto extension : extensions) {
            instance_builder.enable_extension(extension);
        }

        auto instance_ret = instance_builder.build();

        // simple error checking and helpful error messages
        if (!instance_ret) {
            std::cerr << "Failed to create Vulkan instance. Error: " << instance_ret.error().message() << "\n";
            return;
        }

        // Get handle and use however you want!
        instance = instance_ret.value();
        vkbInstance = instance_ret.value();


        volkInitialize();
        volkLoadInstance(instance);
    }

    void RendererRayTraced::pickPhysicalDevice() {
        vkb::PhysicalDeviceSelector selector{ vkbInstance };

        VkPhysicalDeviceFeatures requiredFeatures{};
        requiredFeatures.samplerAnisotropy = VK_TRUE;
        requiredFeatures.shaderStorageImageReadWithoutFormat = VK_TRUE;
        requiredFeatures.shaderStorageImageWriteWithoutFormat = VK_TRUE;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
        };
        accelFeatures.accelerationStructure = VK_TRUE;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
        };
        rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

        VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES
        };
        bdaFeatures.bufferDeviceAddress = VK_TRUE;

        //requires vulkan 1.2
        VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
        indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
        indexingFeatures.descriptorBindingVariableDescriptorCount = VK_TRUE;
        indexingFeatures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        indexingFeatures.runtimeDescriptorArray = VK_TRUE;
        indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        indexingFeatures.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;


        // Build the selector with your requirements
        auto phys_device_ret = selector
            .set_surface(surface)
            .require_present()                          // we want presentation support
            .set_minimum_version(1, 2)                  // require Vulkan 1.0 or higher
            .require_dedicated_transfer_queue()         // optional: ensure transfer queue
            .add_required_extensions(deviceExtensions)  // enable the extensions you want
            .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
            .set_required_features(requiredFeatures)
            .add_required_extension_features(indexingFeatures)
            .add_required_extension_features(accelFeatures)
            .add_required_extension_features(rtPipelineFeatures)
            .add_required_extension_features(bdaFeatures)
            .select();

        // Handle errors
        if (!phys_device_ret) {
            throw std::runtime_error(
                std::string("Failed to select suitable GPU: ") + phys_device_ret.error().message()
            );
        }

        // Store both the vk-bootstrap object and the raw handle
        vkbphysicalDevice = phys_device_ret.value();
        physicalDevice = vkbphysicalDevice.physical_device;

        VkPhysicalDeviceProperties2 prop2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        m_rtProperties.pNext = &m_asProperties;
        prop2.pNext = &m_rtProperties;
        vkGetPhysicalDeviceProperties2(physicalDevice, &prop2);
    }

    void RendererRayTraced::createLogicalDevice() {
        // Build the logical device
        vkb::DeviceBuilder deviceBuilder{ vkbphysicalDevice };

        vkbDevice = deviceBuilder.build().value();

        // Extract Vulkan handles
        device = vkbDevice.device;

        // Retrieve queues
        graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        presentQueue = vkbDevice.get_queue(vkb::QueueType::present).value();

        graphicsQueueIndex = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
        presentQueueIndex = vkbDevice.get_queue_index(vkb::QueueType::present).value();

        volkLoadDevice(device);
    }



}
