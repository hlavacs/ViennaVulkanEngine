
#include "VHInclude.h"
#include "VEInclude.h"


namespace vve {
    RendererRayTraced::RendererRayTraced(std::string systemName, Engine& engine, std::string windowName)
        : System{ systemName, engine }, m_windowName(windowName) {

        
        engine.RegisterCallbacks({
            {this,  3500, "INIT", [this](Message& message) { return OnInit(message); } },
            {this,  2000, "PREPARE_NEXT_FRAME", [this](Message& message) { return OnPrepareNextFrame(message); } },
            {this,  2000, "RECORD_NEXT_FRAME", [this](Message& message) { return OnRecordNextFrame(message); } },
            {this,  2000, "OBJECT_CREATE", [this](Message& message) { return OnObjectCreate(message); } },
            {this, 10000, "OBJECT_DESTROY", [this](Message& message) { return OnObjectDestroy(message); } },
            {this,     0, "QUIT", [this](Message& message) { return OnQuit(message); } }
            });
            
    };

    RendererRayTraced::~RendererRayTraced() {}

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

        textureManager = new TextureManager(device, physicalDevice, commandManager);
        materialManager = new MaterialManager(device, physicalDevice, commandManager);

        materialManager->createMaterialBuffer();
        objectManager = new ObjectManager(device, physicalDevice, commandManager, m_asProperties);

        objectManager->createVertexBuffer();
        objectManager->createIndexBuffer();
        objectManager->createInstanceBuffers();

        objectManager->createBottomLevelAS();
        objectManager->createTopLevelAS();

        uniformBuffer_c.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            uniformBuffer_c[i] = new HostBuffer<UniformBufferObject>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0, device, physicalDevice);
        }

        createDescriptorSetLayout(descriptorSetLayout, device);
        createDescriptorPool(descriptorPool, device);
        createDescriptorSets(descriptorSets, descriptorPool, descriptorSetLayout, uniformBuffer_c, materialManager->getMaterialBuffer(), textureManager->getTextures(), textureManager->getSampler(), device);

        rasterizer = new PiplineRasterized(device, swapchain->getExtent(), commandManager, objectManager->getVertexBuffer(), objectManager->getIndexBuffer(), objectManager->getInstanceBuffers(), descriptorSetLayout, descriptorSets);

        mainTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, swapchain->getFormat(), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        depthTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, commandManager, device, physicalDevice);


        albedoTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        normalTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        specTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        positionTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);

        //rasterizer->bindRenderTarget(mainTarget);
        rasterizer->bindRenderTarget(albedoTarget);
        rasterizer->bindRenderTarget(normalTarget);
        rasterizer->bindRenderTarget(specTarget);
        rasterizer->bindRenderTarget(positionTarget);

        rasterizer->bindDepthRenderTarget(depthTarget);
        rasterizer->initGraphicsPipeline();

        RtTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);

        std::vector<RenderTarget*> targets;

        targets.push_back(albedoTarget);
        targets.push_back(normalTarget);
        targets.push_back(specTarget);
        targets.push_back(positionTarget);

        targets.push_back(RtTarget);



        createDescriptorSetLayoutRT(descriptorSetLayoutRT, device);
        createDescriptorPoolRT(descriptorPoolRT, device);
        createDescriptorSetsRT(descriptorSetsRT, descriptorPoolRT, descriptorSetLayoutRT, objectManager->getTlas().accel, objectManager->getVertexBuffer(), objectManager->getIndexBuffer(), objectManager->getInstanceBuffers(), device);


        createDescriptorSetLayoutTargets(descriptorSetLayoutTargets, targets.size(), device);
        createDescriptorPoolTargets(descriptorPoolTargets, targets.size(), device);
        createDescriptorSetsTargets(descriptorSetsTargets, descriptorPoolTargets, descriptorSetLayoutTargets, targets, device);

        raytracer = new PiplineRaytraced(device, physicalDevice, commandManager, m_rtProperties, descriptorSetLayout, descriptorSets, descriptorSetLayoutRT, descriptorSetsRT, descriptorSetLayoutTargets, descriptorSetsTargets, swapchain->getExtent());

        raytracer->bindRenderTarget(albedoTarget);
        raytracer->bindRenderTarget(normalTarget);
        raytracer->bindRenderTarget(specTarget);
        raytracer->bindRenderTarget(positionTarget);

        raytracer->bindRenderTarget(RtTarget);

        raytracer->initRayTracingPipeline();
        return false;
    }

    bool RendererRayTraced::OnRecordNextFrame(Message message) {

        commandManager->waitForFence(currentFrame);

        VkResult result = swapchain->acquireNextImage(currentFrame);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            swapchain->recreateSwapChain();
            //recreate render targets
            rasterizer->recreateFrameBuffers(swapchain->getExtent());
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        commandManager->beginCommand(currentFrame);
        rasterizer->recordCommandBuffer(objectManager->getobjects(), currentFrame);
        //the raytracer switches the sampler of the textures to linear causing the image to darken. In generell later on everything in the renderer should be switched to linear and only be converted to srgb before presentation
        raytracer->recordCommandBuffer(currentFrame);
        swapchain->recordImageTransfer(currentFrame, RtTarget);
        // swapchain_c->recordImageTransfer(currentFrame, mainTarget);

        commandManager->executeCommand(currentFrame, swapchain->getImageAvailableSemaphore(currentFrame));

        result = swapchain->presentImage(currentFrame, commandManager->getRenderFinishedSemaphores(currentFrame));

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            //recreate render Targets
            swapchain->recreateSwapChain();
            rasterizer->recreateFrameBuffers(swapchain->getExtent());
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        return false;
    }

    bool RendererRayTraced::OnPrepareNextFrame(Message message) {
        return false;
    }
    bool RendererRayTraced::OnObjectCreate(Message message) {
        return false;
    }
    bool RendererRayTraced::OnObjectDestroy(Message message) {
        return false;
    }
    bool RendererRayTraced::OnQuit(Message message) {
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
            .set_minimum_version(1, 0)                  // require Vulkan 1.0 or higher
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
    }



}