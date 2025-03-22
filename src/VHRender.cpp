
#include "VHInclude.h"


namespace vh {

    void RenCreateRenderPassClear(VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain, bool clear, VkRenderPass& renderPass) {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChain.m_swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = RenFindDepthFormat(physicalDevice);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void RenCreateRenderPass(VkPhysicalDevice physicalDevice, VkDevice device, SwapChain& swapChain, bool clear, VkRenderPass& renderPass) {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChain.m_swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = RenFindDepthFormat(physicalDevice);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }


    void RenCreateDescriptorSetLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bind, VkDescriptorSetLayout& descriptorSetLayouts) {
		uint32_t i = 0;
		std::vector<VkDescriptorSetLayoutBinding> bindings = bind;
		for( auto& uboLayoutBinding : bindings ) {
	        uboLayoutBinding.binding = i;
	        uboLayoutBinding.descriptorCount = 1;
	        uboLayoutBinding.pImmutableSamplers = nullptr;
            ++i;
		}

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayouts) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout 0!");
        }
    }

    VkShaderModule RenCreateShaderModule(VkDevice device, const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    void RenCreateGraphicsPipeline(VkDevice device, VkRenderPass renderPass, 
			std::string vertShaderPath, std::string fragShaderPath,
			std::vector<VkVertexInputBindingDescription> bindingDescription, 
			std::vector<VkVertexInputAttributeDescription> attributeDescriptions,
			std::vector<VkDescriptorSetLayout> descriptorSetLayouts, 
			std::vector<int32_t> specializationConstants,
			Pipeline& graphicsPipeline) {

	    // Specialization constant setup
	    std::vector<VkSpecializationMapEntry> specializationEntries;
		for( uint32_t i=0; i<specializationConstants.size(); i++ ) {
			specializationEntries.push_back( 
				VkSpecializationMapEntry{.constantID = i, .offset = i * (uint32_t)sizeof(int32_t), .size = (uint32_t)sizeof(int32_t)} );
		}
		
	    VkSpecializationInfo specializationInfo{};
	    specializationInfo.mapEntryCount = (uint32_t)specializationEntries.size();
	    specializationInfo.pMapEntries = specializationEntries.data();
	    specializationInfo.dataSize = sizeof(int32_t) * specializationConstants.size();
	    specializationInfo.pData = specializationConstants.data();

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        auto vertShaderCode = VulReadFile(vertShaderPath);
        VkShaderModule vertShaderModule = RenCreateShaderModule(device, vertShaderCode);
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
		vertShaderStageInfo.pSpecializationInfo = &specializationInfo;
		shaderStages.push_back(vertShaderStageInfo);

		VkShaderModule fragShaderModule{};
		if( !fragShaderPath.empty() ) {
	        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	        auto fragShaderCode = VulReadFile(fragShaderPath);
	        fragShaderModule = RenCreateShaderModule(device, fragShaderCode);
	        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	        fragShaderStageInfo.module = fragShaderModule;
	        fragShaderStageInfo.pName = "main";
			vertShaderStageInfo.pSpecializationInfo = &specializationInfo;
			shaderStages.push_back(fragShaderStageInfo);
		}

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		if( !fragShaderPath.empty() ) {
	        vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)bindingDescription.size();
	        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	        vertexInputInfo.pVertexBindingDescriptions = bindingDescription.data();
	        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
		}

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; //VK_CULL_MODE_NONE
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT 
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &graphicsPipeline.m_pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = (uint32_t)shaderStages.size();
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = graphicsPipeline.m_pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline.m_pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

		if(shaderStages.size() > 1) { vkDestroyShaderModule(device, fragShaderModule, nullptr); }
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    void RenCreateFramebuffers(VkDevice device, SwapChain& swapChain, DepthImage& depthImage, VkRenderPass renderPass) {
        swapChain.m_swapChainFramebuffers.resize(swapChain.m_swapChainImageViews.size());

        for (size_t i = 0; i < swapChain.m_swapChainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {
                swapChain.m_swapChainImageViews[i],
                depthImage.m_depthImageView
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapChain.m_swapChainExtent.width;
            framebufferInfo.height = swapChain.m_swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChain.m_swapChainFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void RenCreateDescriptorPool(VkDevice device, uint32_t sizes, VkDescriptorPool& descriptorPool) {

		std::vector<VkDescriptorPoolSize> pool_sizes;
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLER, sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, sizes });
		pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, sizes });

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = sizes;
        pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes.data();

		if (vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }


    void RenCreateDescriptorSet(VkDevice device, VkDescriptorSetLayout& descriptorSetLayouts, 
		VkDescriptorPool descriptorPool, DescriptorSet& descriptorSet) {

		descriptorSet.m_descriptorSetPerFrameInFlight.resize(MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayouts);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = (uint32_t)layouts.size();
        allocInfo.pSetLayouts = layouts.data();
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSet.m_descriptorSetPerFrameInFlight.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }
    }


    void RenUpdateDescriptorSet(VkDevice device, Buffer& uniformBuffers, size_t binding, VkDescriptorType type, 
            size_t size, DescriptorSet& descriptorSet) {

		size_t i = 0;
	    for ( auto& ds : descriptorSet.m_descriptorSetPerFrameInFlight ) {
	        VkDescriptorBufferInfo bufferInfo{};
	        bufferInfo.buffer = uniformBuffers.m_uniformBuffers[i++];
	        bufferInfo.offset = 0;
	        bufferInfo.range = size;

	        VkWriteDescriptorSet descriptorWrites{};

	        descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	        descriptorWrites.dstSet = ds;
	        descriptorWrites.dstBinding = (uint32_t)binding;
	        descriptorWrites.dstArrayElement = 0;
	        descriptorWrites.descriptorType = type;
	        descriptorWrites.descriptorCount = 1;
	        descriptorWrites.pBufferInfo = &bufferInfo;

	        vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);
		}
    }

    void RenUpdateDescriptorSetTexture(VkDevice device, Map& texture, size_t binding, DescriptorSet& descriptorSet) {

		size_t i = 0;
	    for ( auto& ds : descriptorSet.m_descriptorSetPerFrameInFlight ) {

	        VkDescriptorImageInfo imageInfo{};
	        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	        imageInfo.imageView = texture.m_mapImageView;
	        imageInfo.sampler = texture.m_mapSampler;

	        VkWriteDescriptorSet descriptorWrites{};

	        descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	        descriptorWrites.dstSet = ds;
	        descriptorWrites.dstBinding = 1;
	        descriptorWrites.dstArrayElement = 0;
	        descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	        descriptorWrites.descriptorCount = 1;
	        descriptorWrites.pImageInfo = &imageInfo;

	        vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);
		}
    }

    void RenCreateDepthResources(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator vmaAllocator
        , SwapChain& swapChain, DepthImage& depthImage) {
        VkFormat depthFormat = RenFindDepthFormat(physicalDevice);

        ImgCreateImage(physicalDevice, device, vmaAllocator, swapChain.m_swapChainExtent.width
            , swapChain.m_swapChainExtent.height, depthFormat
            , VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
            , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage.m_depthImage, depthImage.m_depthImageAllocation);
        depthImage.m_depthImageView = ImgCreateImageView(device, depthImage.m_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    VkFormat RenFindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates
        , VkImageTiling tiling, VkFormatFeatureFlags features) {

        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    VkFormat RenFindDepthFormat(VkPhysicalDevice physicalDevice) {
        return RenFindSupportedFormat(physicalDevice, 
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    bool RenHasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }


} // namespace vh


