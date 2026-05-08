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

        delete commonDescriptors;
        delete reprojectionPassDescriptors;
        delete rtDescriptors;
        delete rtTargetsDescriptors;
        delete combinePassDescriptors;


        //piplines
        rasterizer->freeResources();
        raytracer->freeResources();
        lightVertexGenerationFull->freeResources();
        bidirectionalPathTracing->freeResources();
        combinePass->freeResources();
        reprojectionPass->freeResources();
        restir_temporal->freeResources();
        restir_spatial->freeResources();
        restirGI_temporal->freeResources();
        restirGI_spatial->freeResources();

        vkDestroySampler(device, targetSampler, nullptr);

        //buffers
        lightManager->freeResources();
        textureManager->freeResources();
        materialManager->freeResources();
        objectManager->freeResources();
        commandManager->freeResources();

        for (HostBuffer<UniformBufferObject>* buffer : uniformBuffer_c) {
            delete buffer;
        }

        for (HostBuffer<BidirectionalUniforms>* buffer : bidirectionalUniformsBuffer) {
            delete buffer;
        }

        //render targets
        for (RenderTarget* target : allTargets) {
            delete target;
        }

        delete reservoirDI_A;
        delete reservoirDI_B;

        delete reservoirGI_A;
        delete reservoirGI_B;

        delete lightVertexCache;

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

    PerFrameDescriptorPlacment* RendererRayTraced::getUniformBufferDescriptorInput(int binding, VkShaderStageFlags stageFlags) {
        std::vector<DescriptorInput*> uniformBufferDescriptorInputs{};
        for (GenericBuffer* buffer : uniformBuffer_c) {
            DescriptorBufferInput* descriptorInput = new DescriptorBufferInput(buffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stageFlags);
            uniformBufferDescriptorInputs.push_back(descriptorInput);
        }
        return new PerFrameDescriptorPlacment(uniformBufferDescriptorInputs, binding);
    }

    PerFrameDescriptorPlacment* RendererRayTraced::getBidirectionalUniformBufferDescriptorInput(int binding, VkShaderStageFlags stageFlags) {
        std::vector<DescriptorInput*> uniformBufferDescriptorInputs{};
        for (GenericBuffer* buffer : bidirectionalUniformsBuffer) {
            DescriptorBufferInput* descriptorInput = new DescriptorBufferInput(buffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stageFlags);
            uniformBufferDescriptorInputs.push_back(descriptorInput);
        }
        return new PerFrameDescriptorPlacment(uniformBufferDescriptorInputs, binding);
    }

    void RendererRayTraced::createCommonDescriptors() {
        commonDescriptors = new DescriptorManager(device);
        
        PerFrameDescriptorPlacment* uniformBufferDescriptors = getUniformBufferDescriptorInput(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        commonDescriptors->addDescriptorInput(uniformBufferDescriptors);
        commonDescriptors->addDescriptorInput(materialManager->getMaterialDescriptorInput(1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        commonDescriptors->addDescriptorInput(textureManager->getTextureDescriptorInput(2, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        commonDescriptors->finalize();
    }

    void RendererRayTraced::createRtDescriptors() {
        rtDescriptors = new DescriptorManager(device);

        rtDescriptors->addDescriptorInput(objectManager->getTlasDescriptorInput(0, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR));
        rtDescriptors->addDescriptorInput(objectManager->getVertexDescriptorInput(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR));
        rtDescriptors->addDescriptorInput(objectManager->getIndexDescriptorInput(2, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR));
        rtDescriptors->addDescriptorInput(objectManager->getInstanceDescriptorInput(3, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR));
        rtDescriptors->addDescriptorInput(lightManager->getLightDescriptorInput(4, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR));

        rtDescriptors->finalize();
    }

    void RendererRayTraced::createRtTargetsDescriptors() {
        rtTargetsDescriptors = new DescriptorManager(device);

        rtTargetsDescriptors->addDescriptorInput(albedoTarget->getDescriptorInput(0, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        rtTargetsDescriptors->addDescriptorInput(normalTarget->getDescriptorInput(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        rtTargetsDescriptors->addDescriptorInput(specTarget->getDescriptorInput(2, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        rtTargetsDescriptors->addDescriptorInput(positionTarget->getDescriptorInput(3, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        rtTargetsDescriptors->addDescriptorInput(shadingNormalTarget->getDescriptorInput(4, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        rtTargetsDescriptors->addDescriptorInput(RtTarget->getDescriptorInput(5, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        rtTargetsDescriptors->finalize();
        rtTargetsDescriptors->update();
    }

    void RendererRayTraced::createBidirectionalTargetsDescriptors() {
        bidirectionalPathTracingDescriptors = new DescriptorManager(device);

        bidirectionalPathTracingDescriptors->addDescriptorInput(albedoTarget->getDescriptorInput(0, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        bidirectionalPathTracingDescriptors->addDescriptorInput(normalTarget->getDescriptorInput(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        bidirectionalPathTracingDescriptors->addDescriptorInput(specTarget->getDescriptorInput(2, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        bidirectionalPathTracingDescriptors->addDescriptorInput(positionTarget->getDescriptorInput(3, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        bidirectionalPathTracingDescriptors->addDescriptorInput(shadingNormalTarget->getDescriptorInput(4, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        bidirectionalPathTracingDescriptors->addDescriptorInput(RtTarget->getDescriptorInput(5, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        bidirectionalPathTracingDescriptors->addDescriptorInput(lightVertexCache->getDescriptorInput(6, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        PerFrameDescriptorPlacment* uniformBidirectionalBufferDescriptors = getBidirectionalUniformBufferDescriptorInput(7, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
        bidirectionalPathTracingDescriptors->addDescriptorInput(uniformBidirectionalBufferDescriptors);

        bidirectionalPathTracingDescriptors->finalize();
        bidirectionalPathTracingDescriptors->update();
    }

    void RendererRayTraced::createLightVertexGenerationDescriptors() {
        lightVertexGenerationFullDescriptors = new DescriptorManager(device);

        lightVertexGenerationFullDescriptors->addDescriptorInput(lightVertexCache->getDescriptorInput(0, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        lightVertexGenerationFullDescriptors->finalize();
        lightVertexGenerationFullDescriptors->update();
    }

    void RendererRayTraced::createCombinePassDescriptors() {
        combinePassDescriptors = new DescriptorManager(device);

        combinePassDescriptors->addDescriptorInput(RtTarget->getDescriptorInput(0, VK_SHADER_STAGE_COMPUTE_BIT));
        combinePassDescriptors->addDescriptorInput(albedoTarget->getDescriptorInput(1, VK_SHADER_STAGE_COMPUTE_BIT));
        combinePassDescriptors->addDescriptorInput(lightingReprojectedTarget->getDescriptorInput(2, VK_SHADER_STAGE_COMPUTE_BIT));
        combinePassDescriptors->addDescriptorInput(lightingPreviousTarget->getDescriptorInput(3, VK_SHADER_STAGE_COMPUTE_BIT));
        combinePassDescriptors->addDescriptorInput(reprojectionErrorTarget->getDescriptorInput(4, VK_SHADER_STAGE_COMPUTE_BIT));
        combinePassDescriptors->addDescriptorInput(combinedTarget->getDescriptorInput(5, VK_SHADER_STAGE_COMPUTE_BIT));
        combinePassDescriptors->addDescriptorInput(accumulatedLightingTarget->getDescriptorInput(6, VK_SHADER_STAGE_COMPUTE_BIT));
        

        combinePassDescriptors->finalize();
        combinePassDescriptors->update();
    }

    void RendererRayTraced::createReprojectPassDescriptors() {
        reprojectionPassDescriptors = new DescriptorManager(device);

        PerFrameDescriptorPlacment* uniformBufferDescriptors = getUniformBufferDescriptorInput(0, VK_SHADER_STAGE_COMPUTE_BIT);
        reprojectionPassDescriptors->addDescriptorInput(uniformBufferDescriptors);
        reprojectionPassDescriptors->addDescriptorInput(positionTarget->getDescriptorInput(1, VK_SHADER_STAGE_COMPUTE_BIT));
        reprojectionPassDescriptors->addDescriptorInput(positionPreviousTarget->getDescriptorInput(2, VK_SHADER_STAGE_COMPUTE_BIT, targetSampler));
        reprojectionPassDescriptors->addDescriptorInput(lightingPreviousTarget->getDescriptorInput(3, VK_SHADER_STAGE_COMPUTE_BIT, targetSampler));
        reprojectionPassDescriptors->addDescriptorInput(lightingReprojectedTarget->getDescriptorInput(4, VK_SHADER_STAGE_COMPUTE_BIT));
        reprojectionPassDescriptors->addDescriptorInput(reprojectionErrorTarget->getDescriptorInput(5, VK_SHADER_STAGE_COMPUTE_BIT));

        reprojectionPassDescriptors->addDescriptorInput(positionReprojectedTarget->getDescriptorInput(6, VK_SHADER_STAGE_COMPUTE_BIT));

        reprojectionPassDescriptors->addDescriptorInput(reservoirDI_A->getDescriptorInput(7, VK_SHADER_STAGE_COMPUTE_BIT));
        reprojectionPassDescriptors->addDescriptorInput(reservoirDI_B->getDescriptorInput(8, VK_SHADER_STAGE_COMPUTE_BIT));

        reprojectionPassDescriptors->addDescriptorInput(reservoirGI_A->getDescriptorInput(9, VK_SHADER_STAGE_COMPUTE_BIT));
        reprojectionPassDescriptors->addDescriptorInput(reservoirGI_B->getDescriptorInput(10, VK_SHADER_STAGE_COMPUTE_BIT));


        reprojectionPassDescriptors->finalize();
        reprojectionPassDescriptors->update();
    }

    void RendererRayTraced::createRestirTemporalDescriptors() {
        restir_temporal_descriptors = new DescriptorManager(device);

        restir_temporal_descriptors->addDescriptorInput(albedoTarget->getDescriptorInput(0, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restir_temporal_descriptors->addDescriptorInput(normalTarget->getDescriptorInput(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restir_temporal_descriptors->addDescriptorInput(specTarget->getDescriptorInput(2, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restir_temporal_descriptors->addDescriptorInput(positionTarget->getDescriptorInput(3, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restir_temporal_descriptors->addDescriptorInput(shadingNormalTarget->getDescriptorInput(4, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restir_temporal_descriptors->addDescriptorInput(reprojectionErrorTarget->getDescriptorInput(5, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restir_temporal_descriptors->addDescriptorInput(reservoirDI_B->getDescriptorInput(6, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restir_temporal_descriptors->addDescriptorInput(reservoirDI_A->getDescriptorInput(7, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restir_temporal_descriptors->finalize();
        restir_temporal_descriptors->update();
    }

    void RendererRayTraced::createRestirSpatialDescriptors() {
        restir_spatial_descriptors = new DescriptorManager(device);

        restir_spatial_descriptors->addDescriptorInput(albedoTarget->getDescriptorInput(0, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restir_spatial_descriptors->addDescriptorInput(normalTarget->getDescriptorInput(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restir_spatial_descriptors->addDescriptorInput(specTarget->getDescriptorInput(2, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restir_spatial_descriptors->addDescriptorInput(positionTarget->getDescriptorInput(3, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restir_spatial_descriptors->addDescriptorInput(shadingNormalTarget->getDescriptorInput(4, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restir_spatial_descriptors->addDescriptorInput(RtTarget->getDescriptorInput(5, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restir_spatial_descriptors->addDescriptorInput(reservoirDI_A->getDescriptorInput(6, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restir_spatial_descriptors->addDescriptorInput(reservoirDI_B->getDescriptorInput(7, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restir_spatial_descriptors->finalize();
        restir_spatial_descriptors->update();
    }

    void RendererRayTraced::createRestirGITemporalDescriptors() {
        std::cout << "got function \n";
        restirGI_temporal_descriptors = new DescriptorManager(device);

        restirGI_temporal_descriptors->addDescriptorInput(albedoTarget->getDescriptorInput(0, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restirGI_temporal_descriptors->addDescriptorInput(normalTarget->getDescriptorInput(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restirGI_temporal_descriptors->addDescriptorInput(specTarget->getDescriptorInput(2, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restirGI_temporal_descriptors->addDescriptorInput(positionTarget->getDescriptorInput(3, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restirGI_temporal_descriptors->addDescriptorInput(shadingNormalTarget->getDescriptorInput(4, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restirGI_temporal_descriptors->addDescriptorInput(reprojectionErrorTarget->getDescriptorInput(5, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restirGI_temporal_descriptors->addDescriptorInput(positionReprojectedTarget->getDescriptorInput(6, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restirGI_temporal_descriptors->addDescriptorInput(reservoirDI_B->getDescriptorInput(7, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restirGI_temporal_descriptors->addDescriptorInput(reservoirDI_A->getDescriptorInput(8, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        std::cout << "got here \n";

        restirGI_temporal_descriptors->addDescriptorInput(reservoirGI_B->getDescriptorInput(9, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restirGI_temporal_descriptors->addDescriptorInput(reservoirGI_A->getDescriptorInput(10, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restirGI_temporal_descriptors->finalize();
        restirGI_temporal_descriptors->update();

        std::cout << "got to end \n";
    }

    void RendererRayTraced::createRestirGISpatialDescriptors() {
        restirGI_spatial_descriptors = new DescriptorManager(device);

        restirGI_spatial_descriptors->addDescriptorInput(albedoTarget->getDescriptorInput(0, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restirGI_spatial_descriptors->addDescriptorInput(normalTarget->getDescriptorInput(1, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restirGI_spatial_descriptors->addDescriptorInput(specTarget->getDescriptorInput(2, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restirGI_spatial_descriptors->addDescriptorInput(positionTarget->getDescriptorInput(3, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restirGI_spatial_descriptors->addDescriptorInput(shadingNormalTarget->getDescriptorInput(4, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
        restirGI_spatial_descriptors->addDescriptorInput(RtTarget->getDescriptorInput(5, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restirGI_spatial_descriptors->addDescriptorInput(reservoirDI_A->getDescriptorInput(6, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restirGI_spatial_descriptors->addDescriptorInput(reservoirDI_B->getDescriptorInput(7, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restirGI_spatial_descriptors->addDescriptorInput(reservoirGI_A->getDescriptorInput(8, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restirGI_spatial_descriptors->addDescriptorInput(reservoirGI_B->getDescriptorInput(9, VK_SHADER_STAGE_RAYGEN_BIT_KHR));

        restirGI_spatial_descriptors->finalize();
        restirGI_spatial_descriptors->update();
    }

    void RendererRayTraced::createRenderTargetSampler() {
         VkPhysicalDeviceProperties properties{};
         vkGetPhysicalDeviceProperties(physicalDevice, &properties);

         VkSamplerCreateInfo samplerInfo{};
         samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

         samplerInfo.magFilter = VK_FILTER_LINEAR;
         samplerInfo.minFilter = VK_FILTER_LINEAR;

         samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
         samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
         samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

         samplerInfo.anisotropyEnable = VK_FALSE;
         samplerInfo.maxAnisotropy = 1.0f;

         samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
         samplerInfo.unnormalizedCoordinates = VK_FALSE;
         samplerInfo.compareEnable = VK_FALSE;
         samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

         samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
         samplerInfo.mipLodBias = 0.0f;
         samplerInfo.minLod = 0.0f;
         samplerInfo.maxLod = 0.0f;

         if (vkCreateSampler(device, &samplerInfo, nullptr, &targetSampler) != VK_SUCCESS) {
             throw std::runtime_error("failed to create texture sampler!");
         }
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

        lightVertexCacheSize = VkExtent2D(100000, 1);

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

        bidirectionalUniformsBuffer.resize(MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            bidirectionalUniformsBuffer[i] = new HostBuffer<BidirectionalUniforms>(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0, device, physicalDevice);
        }


        //rasterizer = new PiplineRasterized(device, swapchain->getExtent(), commandManager, objectManager->getVertexBuffer(), objectManager->getIndexBuffer(), objectManager->getInstanceBuffers(), descriptorSetLayout, descriptorSets);

        //mainTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, swapchain->getFormat(), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        depthTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, commandManager, device, physicalDevice);


        albedoTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        normalTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        specTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        positionTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice, VkClearColorValue{{-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),1.0}});
        shadingNormalTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);

        //reprojection
        lightingPreviousTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        lightingReprojectedTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        positionPreviousTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice, VkClearColorValue{ {-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),1.0} });
        reprojectionErrorTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        positionReprojectedTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice, VkClearColorValue{ {-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),1.0} });
        //restir


        // VK_BUFFER_USAGE_TRANSFER_DST_BIT not needed becasue all device buffers are VK_BUFFER_USAGE_TRANSFER_DST_BIT
        reservoirDI_A = new RenderTargetBuffer(swapchain->getExtent().width, swapchain->getExtent().height, ReservoirDI(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, commandManager, device, physicalDevice);
        reservoirDI_B = new RenderTargetBuffer(swapchain->getExtent().width, swapchain->getExtent().height, ReservoirDI(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, commandManager, device, physicalDevice);

        std::cout << "ReservoirGI size: " << sizeof(ReservoirGI) << "\n";

        reservoirGI_A = new RenderTargetBuffer(swapchain->getExtent().width, swapchain->getExtent().height, ReservoirGI(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, commandManager, device, physicalDevice);
        reservoirGI_B = new RenderTargetBuffer(swapchain->getExtent().width, swapchain->getExtent().height, ReservoirGI(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, commandManager, device, physicalDevice);

        lightVertexCache = new RenderTargetBuffer(lightVertexCacheSize.width, 1, LightVertex(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, commandManager, device, physicalDevice);

        //raytracing
        RtTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);

        //accumulation

        combinedTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);
        accumulatedLightingTarget = new RenderTarget(swapchain->getExtent().width, swapchain->getExtent().height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_ASPECT_COLOR_BIT, commandManager, device, physicalDevice);

        allTargets.push_back(albedoTarget);
        allTargets.push_back(normalTarget);
        allTargets.push_back(specTarget);
        allTargets.push_back(positionTarget);
        allTargets.push_back(depthTarget);
        allTargets.push_back(shadingNormalTarget);

        allTargets.push_back(RtTarget);


        allTargets.push_back(lightingPreviousTarget);
        allTargets.push_back(lightingReprojectedTarget);
        allTargets.push_back(positionPreviousTarget);
        allTargets.push_back(reprojectionErrorTarget);

        allTargets.push_back(combinedTarget);
        allTargets.push_back(accumulatedLightingTarget);
        allTargets.push_back(positionReprojectedTarget);


        //rasterizer pipline
        createRenderTargetSampler();

        createCommonDescriptors();

        auto piplineRasterizedUnique = std::make_unique<PiplineRasterized>("Pipline Rasterized", m_engine, device, swapchain->getExtent(), commandManager, objectManager->getVertexBuffer(), objectManager->getIndexBuffer(), objectManager->getInstanceBuffers(), commonDescriptors);
        rasterizer = piplineRasterizedUnique.get();
        m_engine.RegisterSystem(std::move(piplineRasterizedUnique));

        rasterizer->bindRenderTarget(albedoTarget);
        rasterizer->bindRenderTarget(normalTarget);
        rasterizer->bindRenderTarget(specTarget);
        rasterizer->bindRenderTarget(positionTarget);
        rasterizer->bindRenderTarget(shadingNormalTarget);

        rasterizer->bindDepthRenderTarget(depthTarget);
        rasterizer->initGraphicsPipeline();





        rayTracingTargets.push_back(albedoTarget);
        rayTracingTargets.push_back(normalTarget);
        rayTracingTargets.push_back(specTarget);
        rayTracingTargets.push_back(positionTarget);
        rayTracingTargets.push_back(shadingNormalTarget);

        rayTracingTargets.push_back(RtTarget);

        createRtDescriptors();
        createRtTargetsDescriptors();


        raytracer = new PiplineRaytraced(device, physicalDevice, commandManager, m_rtProperties, commonDescriptors, rtDescriptors, rtTargetsDescriptors, swapchain->getExtent(), "shaders/PathTracing/raygen_indirect.rgen.spv", VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        

        raytracer->bindRenderTarget(albedoTarget);
        raytracer->bindRenderTarget(normalTarget);
        raytracer->bindRenderTarget(specTarget);
        raytracer->bindRenderTarget(positionTarget);
        raytracer->bindRenderTarget(shadingNormalTarget);

        raytracer->bindRenderTarget(RtTarget);

        raytracer->initRayTracingPipeline();




        createLightVertexGenerationDescriptors();


        lightVertexGenerationFull = new PiplineRaytraced(device, physicalDevice, commandManager, m_rtProperties, commonDescriptors, rtDescriptors, lightVertexGenerationFullDescriptors, lightVertexCacheSize, "shaders/PathTracing/raygen_light_vertex_generation_full.rgen.spv", VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
        lightVertexGenerationFull->initRayTracingPipeline();

        createBidirectionalTargetsDescriptors();


        bidirectionalPathTracing = new PiplineRaytraced(device, physicalDevice, commandManager, m_rtProperties, commonDescriptors, rtDescriptors, bidirectionalPathTracingDescriptors, swapchain->getExtent(), "shaders/PathTracing/raygen_bidirectional.rgen.spv", VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);



        bidirectionalPathTracing->bindRenderTarget(albedoTarget);
        bidirectionalPathTracing->bindRenderTarget(normalTarget);
        bidirectionalPathTracing->bindRenderTarget(specTarget);
        bidirectionalPathTracing->bindRenderTarget(positionTarget);
        bidirectionalPathTracing->bindRenderTarget(shadingNormalTarget);

        bidirectionalPathTracing->bindRenderTarget(RtTarget);

        bidirectionalPathTracing->initRayTracingPipeline();


        createRestirTemporalDescriptors();

        //needs diffrent pipline barrier!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        restir_temporal = new PiplineRaytraced(device, physicalDevice, commandManager, m_rtProperties, commonDescriptors, rtDescriptors, restir_temporal_descriptors, swapchain->getExtent(), "shaders/PathTracing/raygen_restir_temporal.rgen.spv", VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

        restir_temporal->bindRenderTarget(albedoTarget);
        restir_temporal->bindRenderTarget(normalTarget);
        restir_temporal->bindRenderTarget(specTarget);
        restir_temporal->bindRenderTarget(positionTarget);
        restir_temporal->bindRenderTarget(shadingNormalTarget);

        restir_temporal->bindRenderTarget(reprojectionErrorTarget);


        restir_temporal->initRayTracingPipeline();



        createRestirSpatialDescriptors();

        restir_spatial = new PiplineRaytraced(device, physicalDevice, commandManager, m_rtProperties, commonDescriptors, rtDescriptors, restir_spatial_descriptors, swapchain->getExtent(), "shaders/PathTracing/raygen_restir_spatial.rgen.spv", VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        std::cout << "created  restir spatial \n";

        restir_spatial->bindRenderTarget(albedoTarget);
        restir_spatial->bindRenderTarget(normalTarget);
        restir_spatial->bindRenderTarget(specTarget);
        restir_spatial->bindRenderTarget(positionTarget);
        restir_spatial->bindRenderTarget(shadingNormalTarget);

        restir_spatial->bindRenderTarget(RtTarget);

        restir_spatial->initRayTracingPipeline();


        //RestirGI

        createRestirGITemporalDescriptors();
        std::cout << "created  restirGI temp Descriptor \n";

        //needs diffrent pipline barrier!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        restirGI_temporal = new PiplineRaytraced(device, physicalDevice, commandManager, m_rtProperties, commonDescriptors, rtDescriptors, restirGI_temporal_descriptors, swapchain->getExtent(), "shaders/PathTracing/raygen_restirGI_temporal.rgen.spv", VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

        restirGI_temporal->bindRenderTarget(albedoTarget);
        restirGI_temporal->bindRenderTarget(normalTarget);
        restirGI_temporal->bindRenderTarget(specTarget);
        restirGI_temporal->bindRenderTarget(positionTarget);
        restirGI_temporal->bindRenderTarget(shadingNormalTarget);

        restirGI_temporal->bindRenderTarget(reprojectionErrorTarget);

        restirGI_temporal->bindRenderTarget(positionReprojectedTarget);


        restirGI_temporal->initRayTracingPipeline();

        std::cout << "created  restirGI temp pipline \n";


        createRestirGISpatialDescriptors();

        std::cout << "created  restirGI spatial Descriptor \n";

        restirGI_spatial = new PiplineRaytraced(device, physicalDevice, commandManager, m_rtProperties, commonDescriptors, rtDescriptors, restirGI_spatial_descriptors, swapchain->getExtent(), "shaders/PathTracing/raygen_restirGI_spatial.rgen.spv", VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        restirGI_spatial->bindRenderTarget(albedoTarget);
        restirGI_spatial->bindRenderTarget(normalTarget);
        restirGI_spatial->bindRenderTarget(specTarget);
        restirGI_spatial->bindRenderTarget(positionTarget);
        restirGI_spatial->bindRenderTarget(shadingNormalTarget);

        restirGI_spatial->bindRenderTarget(RtTarget);

        restirGI_spatial->initRayTracingPipeline();

        std::cout << "created  restirGI spatial pipline \n";


        createReprojectPassDescriptors();
        reprojectionPass = new PipelineFilter(device, physicalDevice, commandManager, reprojectionPassDescriptors, swapchain->getExtent(), "shaders/PathTracing/reprojectionPass.spv");
        reprojectionPass->bindRenderTarget(positionTarget);
        reprojectionPass->bindRenderTarget(positionPreviousTarget);
        reprojectionPass->bindRenderTarget(lightingPreviousTarget);
        reprojectionPass->bindRenderTarget(lightingReprojectedTarget);
        reprojectionPass->bindRenderTarget(reprojectionErrorTarget);
        reprojectionPass->bindRenderTarget(positionReprojectedTarget);


        reprojectionPass->initComputePipeline();


        createCombinePassDescriptors();
        combinePass = new PipelineFilter(device, physicalDevice, commandManager, combinePassDescriptors, swapchain->getExtent(), "shaders/PathTracing/combinePass.spv");

        combinePass->bindRenderTarget(RtTarget);
        combinePass->bindRenderTarget(albedoTarget);
        combinePass->bindRenderTarget(lightingReprojectedTarget);
        combinePass->bindRenderTarget(lightingPreviousTarget);
        combinePass->bindRenderTarget(reprojectionErrorTarget);
        combinePass->bindRenderTarget(combinedTarget);
        combinePass->bindRenderTarget(accumulatedLightingTarget);

        combinePass->initComputePipeline();


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
        vkDeviceWaitIdle(device);
        swapchain->recreateSwapChain();

        for (RenderTarget* target : allTargets) {
            target->recreateRenderTarget(swapchain->getExtent().width, swapchain->getExtent().height);
        }

        reservoirDI_A->recreateRenderTarget(swapchain->getExtent().width, swapchain->getExtent().height);
        reservoirDI_B->recreateRenderTarget(swapchain->getExtent().width, swapchain->getExtent().height);
        reservoirGI_A->recreateRenderTarget(swapchain->getExtent().width, swapchain->getExtent().height);
        reservoirGI_B->recreateRenderTarget(swapchain->getExtent().width, swapchain->getExtent().height);

        rasterizer->recreateFrameBuffers(swapchain->getExtent());

        rtTargetsDescriptors->update();
        bidirectionalPathTracingDescriptors->update();
        combinePassDescriptors->update();
        reprojectionPassDescriptors->update();
        restir_temporal_descriptors->update(); 
        restir_spatial_descriptors->update();
        restirGI_temporal_descriptors->update();
        restirGI_spatial_descriptors->update();

        raytracer->setExtent(swapchain->getExtent());
        bidirectionalPathTracing->setExtent(swapchain->getExtent());
        restir_temporal->setExtent(swapchain->getExtent());
        restir_spatial->setExtent(swapchain->getExtent());
        restirGI_temporal->setExtent(swapchain->getExtent());
        restirGI_spatial->setExtent(swapchain->getExtent());
        combinePass->setExtent(swapchain->getExtent());
        reprojectionPass->setExtent(swapchain->getExtent());
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
        reprojectionPass->recordCommandBuffer(currentFrame);

        //raytracer->recordCommandBuffer(currentFrame);

        //restir_temporal->recordCommandBuffer(currentFrame);
        //restir_spatial->recordCommandBuffer(currentFrame);

        restirGI_temporal->recordCommandBuffer(currentFrame);
        restirGI_spatial->recordCommandBuffer(currentFrame);

        //lightVertexGenerationFull->recordCommandBuffer(currentFrame);
        //bidirectionalPathTracing->recordCommandBuffer(currentFrame);

        combinePass->recordCommandBuffer(currentFrame);
        //copy images to previous image buffers
        //WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        //Writing to the next image might introduce syncronisation errors.
        //should such errors ever occur, intorduce a ubo vector and ensure that the previous view and projection matrix are read from the ubo of the same frame in flight
        //(and copy to the same frame in flight here)
        lightingPreviousTarget->getImage(nextFrame)->recordCopyFromImage(accumulatedLightingTarget->getImage(currentFrame), VK_IMAGE_LAYOUT_GENERAL, currentFrame);
        positionPreviousTarget->getImage(nextFrame)->recordCopyFromImage(positionTarget->getImage(currentFrame), VK_IMAGE_LAYOUT_GENERAL, currentFrame);

        reservoirDI_A->getBuffer(nextFrame)->recordCopyFromBuffer(reservoirDI_B->getBuffer(currentFrame), currentFrame);
        reservoirGI_A->getBuffer(nextFrame)->recordCopyFromBuffer(reservoirGI_B->getBuffer(currentFrame), currentFrame);

        //lightVertexCache->getBuffer(nextFrame)->recordCopyFromBuffer(lightVertexCache->getBuffer(currentFrame), currentFrame);

        swapchain->recordImageTransfer(currentFrame, combinedTarget);
        //swapchain->recordImageTransfer(currentFrame, albedoTarget);

        return false;
    }

    void RendererRayTraced::updateUniformBuffer(uint32_t currentImage) {

        auto [lToW, view, proj] = *m_registry.GetView<LocalToWorldMatrix&, ViewMatrix&, ProjectionMatrix&>().begin();

        UniformBufferObject ubo{};

        ubo.view = view();
        ubo.proj = proj();

        ubo.prevView = uniforms.view;
        ubo.prevProj = uniforms.proj;

        ubo.viewInv = glm::transpose(glm::inverse(ubo.view)); // Transpose inverse view matrix
        ubo.projInv = glm::transpose(glm::inverse(ubo.proj));

        ubo.seed = dist(gen);
        ubo.lightCount = lightManager->getLightCount();
        ubo.x_dimensions = swapchain->getExtent().width;
        ubo.y_dimensions = swapchain->getExtent().height;

        uniforms = ubo;

        uniformBuffer_c[currentImage]->updateBuffer(&ubo, 1);


        BidirectionalUniforms uboBDPT{};

        uboBDPT.LVCSize = lightVertexCacheSize.width;

        bidirectionalUniformsBuffer[currentImage]->updateBuffer(&uboBDPT, 1);

    }

    bool RendererRayTraced::OnPrepareNextFrame(Message message) {
        commandManager->waitForFence(currentFrame);
        updateUniformBuffer(currentFrame);

        if (materialManager->materialChanged() || textureManager->texturesChanged()) {
            commonDescriptors->destroyDescriptorSets();
        }
        if (objectManager->meshesChanged() || objectManager->instancesChanged() || lightManager->lightsChanged()) {
            rtDescriptors->destroyDescriptorSets();
        }

        lightManager->prepareNextFrame();
        materialManager->prepareNextFrame();
        objectManager->prepareNextFrame();

        if (materialManager->materialChanged() || textureManager->texturesChanged()) {
            commonDescriptors->update();
        }
        if (objectManager->meshesChanged() || objectManager->instancesChanged() || lightManager->lightsChanged()) {
            rtDescriptors->update();
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
        nextFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

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
